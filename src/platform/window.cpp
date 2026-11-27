#include "nopeui/platform/window.h"

#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwrite.h>
#include <imm.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <windowsx.h>
#include <wrl/client.h>

#include "nopeui/controls/tooltip.h"
#include "nopeui/graphics/offscreen.h"
#include "nopeui/graphics/scene.h"
#include "nopeui/layout/style.h"
#include "nopeui/platform/automation.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr const wchar_t* kWindowClassName = L"NopeUI.Window";
constexpr UINT_PTR kCommitTimerId = 1;
constexpr UINT_PTR kLiveResizeTimerId = 2;
constexpr UINT_PTR kResizeSettleTimerId = 3;
constexpr UINT_PTR kToolTipTimerId = 4;
constexpr UINT kToolTipDelayMs = 600;
constexpr UINT kClosePopupMessage = WM_APP + 7;
constexpr UINT kRecreateGraphicsMessage = WM_APP + 8;
constexpr UINT kLiveResizeIntervalMs = 8;
constexpr UINT kResizeSettleIntervalMs = 300;
constexpr std::size_t kMaxQueuedCommits = 1;
constexpr int kMaxAsyncSurfaces = 3;
constexpr int kMinAsyncSurfaces = 2;

double MillisecondsToNextVblank(LONGLONG submit_qpc) noexcept {
    DWM_TIMING_INFO timing{};
    timing.cbSize = sizeof(timing);
    if (FAILED(DwmGetCompositionTimingInfo(nullptr, &timing)) ||
        timing.qpcRefreshPeriod <= 0 ||
        timing.qpcVBlank <= 0) {
        return 0.0;
    }

    LARGE_INTEGER frequency{};
    LARGE_INTEGER now{};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&now)) {
        return 0.0;
    }

    LONGLONG next_vblank = timing.qpcVBlank;
    while (next_vblank <= now.QuadPart) {
        next_vblank += timing.qpcRefreshPeriod;
    }
    const double milliseconds =
        static_cast<double>(next_vblank - submit_qpc) * 1000.0 /
        static_cast<double>(frequency.QuadPart);
    return std::max(0.0, milliseconds);
}

double QueryRefreshIntervalMilliseconds(HWND hwnd) noexcept {
    DWM_TIMING_INFO timing{};
    timing.cbSize = sizeof(timing);
    if (SUCCEEDED(DwmGetCompositionTimingInfo(hwnd, &timing)) && timing.qpcRefreshPeriod > 0) {
        LARGE_INTEGER frequency{};
        if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0) {
            const double milliseconds =
                static_cast<double>(timing.qpcRefreshPeriod) * 1000.0 /
                static_cast<double>(frequency.QuadPart);
            return std::clamp(milliseconds, 1.0, 1000.0);
        }
    }
    return 1000.0 / 120.0;
}

void EnableDpiAwareness() noexcept {
    static std::once_flag once;
    std::call_once(once, [] {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    });
}

float DpiScaleFrom(UINT dpi) noexcept {
    return dpi > 0 ? static_cast<float>(dpi) / 96.0f : 1.0f;
}

UINT QueryWindowDpi(HWND hwnd) noexcept {
    if (hwnd != nullptr) {
        const UINT dpi = GetDpiForWindow(hwnd);
        if (dpi > 0) {
            return dpi;
        }
    }
    const UINT dpi = GetDpiForSystem();
    return dpi > 0 ? dpi : 96;
}

std::wstring ToWideString(std::u16string_view text) {
    std::wstring result;
    result.reserve(text.size());
    for (const char16_t character : text) {
        result.push_back(static_cast<wchar_t>(character));
    }
    return result;
}

std::string PlatformError(const char* operation, HRESULT result) {
    char message[160];
    std::snprintf(
        message,
        sizeof(message),
        "%s failed with HRESULT 0x%08X",
        operation,
        static_cast<unsigned int>(result));
    return message;
}

std::wstring ReadImeString(HWND hwnd, DWORD type) {
    HIMC context = ImmGetContext(hwnd);
    if (context == nullptr) {
        return {};
    }
    const LONG bytes = ImmGetCompositionStringW(context, type, nullptr, 0);
    std::wstring text;
    if (bytes > 0) {
        text.resize(static_cast<std::size_t>(bytes) / sizeof(wchar_t));
        ImmGetCompositionStringW(
            context,
            type,
            text.data(),
            static_cast<DWORD>(bytes));
    }
    ImmReleaseContext(hwnd, context);
    return text;
}

}  // namespace

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

graphics::ScenePalette PaletteFromTheme(const Theme& theme) noexcept {
    graphics::ScenePalette palette;
    palette.background = theme.background;
    palette.accent = theme.accent;
    palette.text = theme.text;
    return palette;
}

}  // namespace

class WindowGraphics {
public:
    static Result<std::unique_ptr<WindowGraphics>> Create(HWND hwnd, int width, int height) {
        std::unique_ptr<WindowGraphics> graphics(new WindowGraphics());
        auto initialized = graphics->Initialize(hwnd, width, height);
        if (!initialized.ok()) {
            return Result<std::unique_ptr<WindowGraphics>>::Fail(initialized.error());
        }
        return Result<std::unique_ptr<WindowGraphics>>::Ok(std::move(graphics));
    }

    ~WindowGraphics() {
        StopCommitThread();
        ReleaseSurface();
        if (visual_ != nullptr) {
            visual_->SetContent(nullptr);
        }
    }

    static int AllocationExtent(int value) noexcept {
        const int slack = value <= 2048 ? value / 3 : 256;
        return value + slack;
    }

    // Drawing happens in DIPs; surfaces and DXGI update rects are in pixels.
    float DpiScale() const noexcept {
        return dpi_ / 96.0f;
    }

    void SetDpi(float dpi) noexcept {
        if (dpi <= 0.0f) {
            return;
        }
        if (std::abs(dpi - dpi_) < 0.5f) {
            return;
        }
        dpi_ = dpi;
        if (d2d_context_ != nullptr) {
            d2d_context_->SetDpi(dpi_, dpi_);
        }
        for (auto& state : surfaces_) {
            state.target_bitmap.Reset();
        }
        swap_state_.target_bitmap.Reset();
        target_bound_index_ = -1;
        software_bitmap_.Reset();
        software_surface_.reset();
        software_painter_.reset();
        if (pixel_width_ > 0 && pixel_height_ > 0) {
            content_width_ = DipExtent(pixel_width_);
            content_height_ = DipExtent(pixel_height_);
        }
        full_redraw_required_ = true;
    }

    int PixelWidth() const noexcept {
        return pixel_width_ > 0 ? pixel_width_ : surface_width_;
    }

    int PixelHeight() const noexcept {
        return pixel_height_ > 0 ? pixel_height_ : surface_height_;
    }

    int DipExtent(int pixels) const noexcept {
        return static_cast<int>(
            std::lround(static_cast<double>(pixels) * 96.0 / static_cast<double>(dpi_)));
    }

    int SurfaceCountFor(int width, int height) const noexcept {
        if (forced_surface_count_ > 0) {
            return forced_surface_count_;
        }
        constexpr std::uint64_t triple_limit =
            static_cast<std::uint64_t>(2560) * static_cast<std::uint64_t>(1600);
        const std::uint64_t area =
            static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
        return area <= triple_limit ? kMaxAsyncSurfaces : kMinAsyncSurfaces;
    }

    Result<void> CreateFlipSwapChain(int width, int height) noexcept {
        if (swap_chain_ != nullptr) {
            if (d2d_context_ != nullptr) {
                d2d_context_->SetTarget(nullptr);
            }
            swap_state_.target_bitmap.Reset();
            target_bound_index_ = -1;
            HRESULT result = swap_chain_->ResizeBuffers(
                0,
                static_cast<UINT>(width),
                static_cast<UINT>(height),
                DXGI_FORMAT_UNKNOWN,
                0);
            if (FAILED(result)) {
                return Result<void>::Fail(
                    Error{ErrorCode::PlatformFailure, PlatformError("ResizeBuffers", result)});
            }
        } else {
            ComPtr<IDXGIAdapter> adapter;
            HRESULT result = dxgi_device_->GetAdapter(adapter.GetAddressOf());
            if (FAILED(result)) {
                return Result<void>::Fail(
                    Error{ErrorCode::PlatformFailure, PlatformError("GetAdapter", result)});
            }
            ComPtr<IDXGIFactory2> factory;
            result = adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
            if (FAILED(result)) {
                return Result<void>::Fail(
                    Error{ErrorCode::PlatformFailure, PlatformError("GetParent", result)});
            }
            DXGI_SWAP_CHAIN_DESC1 desc{};
            desc.Width = static_cast<UINT>(width);
            desc.Height = static_cast<UINT>(height);
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 2;
            desc.Scaling = DXGI_SCALING_STRETCH;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
            result = factory->CreateSwapChainForComposition(
                d3d_device_.Get(), &desc, nullptr, swap_chain_.GetAddressOf());
            if (FAILED(result)) {
                return Result<void>::Fail(
                    Error{ErrorCode::PlatformFailure, PlatformError("CreateSwapChainForComposition", result)});
            }
            result = visual_->SetContent(swap_chain_.Get());
            if (FAILED(result)) {
                return Result<void>::Fail(
                    Error{ErrorCode::PlatformFailure, PlatformError("SetContent(swap chain)", result)});
            }
        }
        swap_state_.target_bitmap.Reset();
        swap_state_.width = width;
        swap_state_.height = height;
        full_redraw_required_ = true;
        return Result<void>::Ok();
    }

    void SetSoftwareRender(bool enabled) noexcept {
        if (software_render_ == enabled) {
            return;
        }
        software_render_ = enabled;
        software_bitmap_.Reset();
        full_redraw_required_ = true;
        Render(nullptr, true);
    }

    bool IsSoftwareRender() const noexcept {
        return software_render_;
    }

    bool TakeDeviceLost() noexcept {
        const bool lost = device_lost_;
        device_lost_ = false;
        return lost;
    }

    void SetContent(Element* content) noexcept {
        content_ = content;
    }

    void SetPalette(const graphics::ScenePalette& palette) noexcept {
        palette_ = palette;
        scene_painter_.reset();
        auto painter = graphics::ScenePainter::Create(d2d_context_.Get(), palette_);
        if (painter.ok()) {
            scene_painter_ = std::move(painter.value());
        }
        software_painter_.reset();
        software_bitmap_.Reset();
        full_redraw_required_ = true;
    }

    void DrawSoftwareScene(const RECT* update_ptr, const D2D1_RECT_F& background_rect) noexcept {
        if (!EnsureSoftwareSurface(PixelWidth(), PixelHeight())) {
            return;
        }
        auto* surface = software_surface_.get();
        if (software_painter_ == nullptr) {
            return;
        }
        if (!surface->BeginDraw().ok()) {
            return;
        }
        const auto painted = software_painter_->Paint(
            static_cast<float>(content_width_),
            static_cast<float>(content_height_),
            background_rect.left,
            background_rect.top,
            background_rect.right,
            background_rect.bottom);
        if (content_ != nullptr) {
            content_->Paint(surface->Target());
        }
        if (!surface->EndDraw().ok() || !painted.ok()) {
            return;
        }

        if (software_bitmap_ == nullptr) {
            const HRESULT result = d2d_context_->CreateBitmapFromWicBitmap(
                surface->WicBitmap(),
                nullptr,
                software_bitmap_.GetAddressOf());
            if (FAILED(result)) {
                return;
            }
        }

        if (update_ptr != nullptr) {
            d2d_context_->PushAxisAlignedClip(background_rect, D2D1_ANTIALIAS_MODE_ALIASED);
        }
        d2d_context_->DrawBitmap(
            software_bitmap_.Get(),
            D2D1::RectF(
                0.0f,
                0.0f,
                static_cast<float>(content_width_),
                static_cast<float>(content_height_)),
            1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        if (update_ptr != nullptr) {
            d2d_context_->PopAxisAlignedClip();
        }
    }

    bool EnsureSoftwareSurface(int width, int height) noexcept {
        if (width <= 0 || height <= 0) {
            return false;
        }
        if (software_surface_ == nullptr ||
            software_surface_->Width() != width ||
            software_surface_->Height() != height) {
            auto created = graphics::OffscreenSurface::Create(width, height, dpi_);
            if (!created.ok()) {
                software_surface_.reset();
                software_painter_.reset();
                software_bitmap_.Reset();
                return false;
            }
            software_surface_ = std::move(created.value());
            software_painter_.reset();
            software_bitmap_.Reset();
        }
        if (software_painter_ == nullptr) {
            auto painter = graphics::ScenePainter::Create(
                software_surface_->Target(),
                palette_);
            if (!painter.ok()) {
                software_surface_.reset();
                software_painter_.reset();
                software_bitmap_.Reset();
                return false;
            }
            software_painter_ = std::move(painter.value());
        }
        return true;
    }

    void SetFlipModel(bool enabled) noexcept {
        if (flip_model_ == enabled) {
            return;
        }
        if (enabled) {
            if (async_commit_) {
                WaitForAllCommits();
                async_commit_ = false;
            }
            for (auto& state : surfaces_) {
                state.surface.Reset();
                state.target_bitmap.Reset();
                state.width = 0;
                state.height = 0;
            }
            surface_count_ = 0;
            target_bound_index_ = -1;
            const int width = PixelWidth();
            const int height = PixelHeight();
            auto created = CreateFlipSwapChain(width, height);
            flip_model_ = created.ok();
            if (flip_model_) {
                dcomp_device_->Commit();
            }
            return;
        }

        if (swap_chain_ != nullptr) {
            visual_->SetContent(nullptr);
            dcomp_device_->Commit();
            swap_chain_.Reset();
        }
        swap_state_.target_bitmap.Reset();
        target_bound_index_ = -1;
        flip_model_ = false;
        const int width = PixelWidth();
        const int height = PixelHeight();
        Resize(width, height, true);
    }

    Result<void> Resize(int width, int height, bool exact = false) noexcept {
        if (width <= 0 || height <= 0) {
            return Result<void>::Ok();
        }
        pixel_width_ = width;
        pixel_height_ = height;

        if (flip_model_) {
            auto resized = CreateFlipSwapChain(width, height);
            if (!resized.ok()) {
                return resized;
            }
            content_width_ = DipExtent(width);
            content_height_ = DipExtent(height);
            surface_width_ = width;
            surface_height_ = height;
            return Result<void>::Ok();
        }

        const int target_width =
            exact ? width : (surface_width_ >= width ? surface_width_ : AllocationExtent(width));
        const int target_height =
            exact ? height : (surface_height_ >= height ? surface_height_ : AllocationExtent(height));

        const int async_surfaces = SurfaceCountFor(width, height);
        const int needed_surfaces = async_commit_ ? async_surfaces : 1;
        if (async_commit_) {
            async_surface_count_ = async_surfaces;
        }
        bool all_target =
            surface_count_ >= needed_surfaces && surface_count_ <= needed_surfaces;
        for (int index = 0; index < needed_surfaces; ++index) {
            const auto& state = surfaces_[index];
            if (state.surface == nullptr || state.width != target_width ||
                state.height != target_height) {
                all_target = false;
                break;
            }
        }
        if (all_target) {
            content_width_ = DipExtent(width);
            content_height_ = DipExtent(height);
            full_redraw_required_ = true;
            return Result<void>::Ok();
        }

        if (async_commit_) {
            PrepareForResize();
        }
        for (int index = 0; index < needed_surfaces; ++index) {
            auto& state = surfaces_[index];
            if (state.surface == nullptr) {
                HRESULT result = dcomp_device_->CreateVirtualSurface(
                    static_cast<UINT>(target_width),
                    static_cast<UINT>(target_height),
                    DXGI_FORMAT_B8G8R8A8_UNORM,
                    DXGI_ALPHA_MODE_PREMULTIPLIED,
                    state.surface.GetAddressOf());
                if (FAILED(result)) {
                    return Result<void>::Fail(
                        Error{ErrorCode::PlatformFailure, PlatformError("CreateVirtualSurface", result)});
                }

                if (index == 0) {
                    result = visual_->SetContent(state.surface.Get());
                    if (FAILED(result)) {
                        return Result<void>::Fail(
                            Error{ErrorCode::PlatformFailure, PlatformError("SetContent", result)});
                    }
                }
            } else {
                HRESULT result =
                    state.surface->Resize(static_cast<UINT>(target_width), static_cast<UINT>(target_height));
                if (FAILED(result)) {
                    return Result<void>::Fail(
                        Error{ErrorCode::PlatformFailure, PlatformError("Resize", result)});
                }
            }

            state.width = target_width;
            state.height = target_height;
            state.target_bitmap.Reset();
        }
        for (int index = needed_surfaces; index < surface_count_; ++index) {
            auto& state = surfaces_[index];
            state.surface.Reset();
            state.target_bitmap.Reset();
            state.width = 0;
            state.height = 0;
        }
        surface_count_ = needed_surfaces;
        if (draw_index_ >= surface_count_) {
            draw_index_ = 0;
        }
        target_bound_index_ = -1;
        full_redraw_required_ = true;
        surface_width_ = target_width;
        surface_height_ = target_height;
        content_width_ = DipExtent(width);
        content_height_ = DipExtent(height);
        return Result<void>::Ok();
    }

    void Render(const RECT* dirty_rect = nullptr, bool force_commit = false) noexcept {
        const auto start = std::chrono::steady_clock::now();
        const auto sequence = ++frame_sequence_;
        if (flip_model_) {
            if (swap_chain_ == nullptr) {
                return;
            }
        } else if (async_commit_) {
            WaitForSurface(draw_index_);
            if (FAILED(EnsureSurfaceSize(draw_index_))) {
                return;
            }
        }
        if (d2d_context_ == nullptr) {
            return;
        }

        auto& surface_state = flip_model_ ? swap_state_ : surfaces_[draw_index_];
        if (!flip_model_ && surface_state.surface == nullptr) {
            return;
        }

        RECT update{};
        RECT update_pixels{};
        const RECT* update_ptr = nullptr;
        if (dirty_rect != nullptr) {
            update.left = std::max(0L, dirty_rect->left);
            update.top = std::max(0L, dirty_rect->top);
            update.right = std::min(static_cast<LONG>(content_width_), dirty_rect->right);
            update.bottom = std::min(static_cast<LONG>(content_height_), dirty_rect->bottom);
            if (update.right <= update.left || update.bottom <= update.top) {
                return;
            }
            const float scale = DpiScale();
            update_pixels.left = static_cast<LONG>(std::lround(update.left * scale));
            update_pixels.top = static_cast<LONG>(std::lround(update.top * scale));
            update_pixels.right = static_cast<LONG>(std::lround(update.right * scale));
            update_pixels.bottom = static_cast<LONG>(std::lround(update.bottom * scale));
            update_ptr = &update_pixels;
        }

        if (full_redraw_required_) {
            update_ptr = nullptr;
            full_redraw_required_ = false;
        }

        const D2D1_RECT_F background_rect = update_ptr == nullptr
            ? D2D1::RectF(0.0f, 0.0f, static_cast<float>(content_width_), static_cast<float>(content_height_))
            : D2D1::RectF(
                  static_cast<float>(update.left),
                  static_cast<float>(update.top),
                  static_cast<float>(update.right),
                  static_cast<float>(update.bottom));

        ComPtr<IDXGISurface> dxgi_surface;
        POINT offset{};
        const auto begin_start = std::chrono::steady_clock::now();
        HRESULT result = E_FAIL;
        if (flip_model_) {
            result = swap_chain_->GetBuffer(
                0, __uuidof(IDXGISurface), reinterpret_cast<void**>(dxgi_surface.GetAddressOf()));
        } else {
            result = surface_state.surface->BeginDraw(
                update_ptr,
                __uuidof(IDXGISurface),
                reinterpret_cast<void**>(dxgi_surface.GetAddressOf()),
                &offset);
        }
        const auto begin_end = std::chrono::steady_clock::now();
        if (FAILED(result)) {
            if (result == D2DERR_RECREATE_TARGET) {
                device_lost_ = true;
            }
            return;
        }

        if (surface_state.target_bitmap == nullptr) {
            D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                dpi_,
                dpi_);
            result = d2d_context_->CreateBitmapFromDxgiSurface(
                dxgi_surface.Get(), properties, surface_state.target_bitmap.GetAddressOf());
            if (FAILED(result)) {
                if (!flip_model_) {
                    surface_state.surface->EndDraw();
                }
                return;
            }
        }

        if (flip_model_) {
            d2d_context_->SetTarget(surface_state.target_bitmap.Get());
            target_bound_index_ = -2;
        } else if (target_bound_index_ != draw_index_) {
            d2d_context_->SetTarget(surface_state.target_bitmap.Get());
            target_bound_index_ = draw_index_;
        }
        const float scale = DpiScale();
        d2d_context_->SetTransform(D2D1::Matrix3x2F::Translation(
            static_cast<float>(offset.x) / scale,
            static_cast<float>(offset.y) / scale));
        d2d_context_->BeginDraw();
        if (software_render_) {
            DrawSoftwareScene(update_ptr, background_rect);
        } else {
            if (update_ptr != nullptr) {
                d2d_context_->PushAxisAlignedClip(background_rect, D2D1_ANTIALIAS_MODE_ALIASED);
            }
            if (scene_painter_ != nullptr) {
                scene_painter_->Paint(
                    static_cast<float>(content_width_),
                    static_cast<float>(content_height_),
                    background_rect.left,
                    background_rect.top,
                    background_rect.right,
                    background_rect.bottom);
            }
            if (content_ != nullptr) {
                content_->Paint(d2d_context_.Get());
            }
            if (diagnostic_rectangles_ > 0) {
                if (diagnostic_virtual_list_) {
                    constexpr std::uint32_t columns = 64;
                    constexpr float row_step = 10.0f;
                    constexpr float first_y = 100.0f;
                    const std::uint32_t total_rows =
                        (diagnostic_rectangles_ + columns - 1) / columns;
                    const int first_row = std::max(
                        0,
                        static_cast<int>(std::floor(
                            (background_rect.top + diagnostic_scroll_ - first_y) / row_step)));
                    const int last_row = std::min(
                        static_cast<int>(total_rows) - 1,
                        static_cast<int>(std::floor(
                            (background_rect.bottom + diagnostic_scroll_ - first_y) / row_step)));
                    for (int row = first_row; row <= last_row; ++row) {
                        for (std::uint32_t column = 0; column < columns; ++column) {
                            const std::uint32_t index =
                                static_cast<std::uint32_t>(row) * columns + column;
                            if (index >= diagnostic_rectangles_) {
                                break;
                            }
                            const float x = 4.0f + static_cast<float>(column) * 10.0f;
                            const float y = first_y + static_cast<float>(row) * row_step -
                                            diagnostic_scroll_;
                            d2d_context_->FillRectangle(
                                D2D1::RectF(x, y, x + 6.0f, y + 6.0f),
                                (index & 1U) != 0 ? accent_brush_.Get() : text_brush_.Get());
                        }
                    }
                } else {
                    for (std::uint32_t index = 0; index < diagnostic_rectangles_; ++index) {
                        const float x = 4.0f + static_cast<float>(index % 64) * 10.0f;
                        const float y = 100.0f + static_cast<float>((index / 64) % 64) * 10.0f;
                        d2d_context_->FillRectangle(
                            D2D1::RectF(x, y, x + 6.0f, y + 6.0f),
                            (index & 1U) != 0 ? accent_brush_.Get() : text_brush_.Get());
                    }
                }
            }
            if (!diagnostic_text_rows_.empty()) {
                constexpr float first_y = 28.0f;
                const std::uint32_t total_rows =
                    static_cast<std::uint32_t>(diagnostic_text_rows_.size());
                const int first_row = std::max(
                    0,
                    static_cast<int>(std::floor(
                        (background_rect.top + diagnostic_scroll_ - first_y) /
                        diagnostic_row_height_)));
                const int last_row = std::min(
                    static_cast<int>(total_rows) - 1,
                    static_cast<int>(std::floor(
                        (background_rect.bottom + diagnostic_scroll_ - first_y) /
                        diagnostic_row_height_)));
                for (int row = first_row; row <= last_row; ++row) {
                    auto layout = DiagnosticLayout(static_cast<std::uint32_t>(row));
                    if (layout == nullptr) {
                        continue;
                    }
                    d2d_context_->DrawTextLayout(
                        D2D1::Point2F(
                            24.0f,
                            first_y + static_cast<float>(row) * diagnostic_row_height_ -
                                diagnostic_scroll_),
                        layout.Get(),
                        text_brush_.Get());
                }
            }
            if (diagnostic_text_runs_ > 0) {
                const auto draw_diagnostic_text = [&](float x, float y) {
                    if (text_bitmap_ != nullptr) {
                        d2d_context_->DrawBitmap(
                            text_bitmap_.Get(),
                            D2D1::RectF(x, y, x + 180.0f, y + 32.0f),
                            1.0f,
                            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
                    } else if (text_layout_ != nullptr) {
                        d2d_context_->DrawTextLayout(
                            D2D1::Point2F(x, y),
                            text_layout_.Get(),
                            text_brush_.Get());
                    }
                };
                if (diagnostic_virtual_list_) {
                    constexpr std::uint32_t columns = 8;
                    constexpr float row_step = 20.0f;
                    constexpr float first_y = 28.0f;
                    const std::uint32_t total_rows =
                        (diagnostic_text_runs_ + columns - 1) / columns;
                    const int first_row = std::max(
                        0,
                        static_cast<int>(std::floor(
                            (background_rect.top + diagnostic_scroll_ - first_y) / row_step)));
                    const int last_row = std::min(
                        static_cast<int>(total_rows) - 1,
                        static_cast<int>(std::floor(
                            (background_rect.bottom + diagnostic_scroll_ - first_y) / row_step)));
                    for (int row = first_row; row <= last_row; ++row) {
                        for (std::uint32_t column = 0; column < columns; ++column) {
                            const std::uint32_t index =
                                static_cast<std::uint32_t>(row) * columns + column;
                            if (index >= diagnostic_text_runs_) {
                                break;
                            }
                            draw_diagnostic_text(
                                24.0f + static_cast<float>(column) * 120.0f,
                                first_y + static_cast<float>(row) * row_step -
                                    diagnostic_scroll_);
                        }
                    }
                } else {
                    for (std::uint32_t index = 0; index < diagnostic_text_runs_; ++index) {
                        const float x = 24.0f + static_cast<float>(index % 8) * 12.0f;
                        const float y = 28.0f + static_cast<float>((index / 8) % 32) * 8.0f;
                        draw_diagnostic_text(x, y);
                    }
                }
            }
            if (update_ptr != nullptr) {
                d2d_context_->PopAxisAlignedClip();
            }
        }
        result = d2d_context_->EndDraw();
        if (FAILED(result)) {
            target_bound_index_ = -1;
            if (result == D2DERR_RECREATE_TARGET) {
                device_lost_ = true;
            }
        }
        const auto draw_end = std::chrono::steady_clock::now();

        if (flip_model_) {
            if (FAILED(result)) {
                return;
            }
            d2d_context_->SetTarget(nullptr);
            surface_state.target_bitmap.Reset();
            target_bound_index_ = -1;
            const auto present_start = std::chrono::steady_clock::now();
            HRESULT present_result = swap_chain_->Present(0, 0);
            const auto present_end = std::chrono::steady_clock::now();
            if (FAILED(present_result)) {
                if (present_result == DXGI_ERROR_DEVICE_REMOVED ||
                    present_result == DXGI_ERROR_DEVICE_RESET) {
                    device_lost_ = true;
                }
                return;
            }
            last_commit_ms_ =
                std::chrono::duration<double, std::milli>(present_end - present_start).count();
            commit_count_.fetch_add(1, std::memory_order_relaxed);
            commit_pending_ = false;
            last_commit_time_ = present_end;

            FrameTimings timings;
            timings.sequence = sequence;
            timings.begin_draw_ms = std::chrono::duration<double, std::milli>(
                begin_end - begin_start).count();
            timings.draw_ms =
                std::chrono::duration<double, std::milli>(draw_end - begin_end).count();
            timings.end_draw_ms = 0.0;
            timings.commit_ms = last_commit_ms_;
            last_timings_ = timings;

            LARGE_INTEGER present_qpc{};
            QueryPerformanceCounter(&present_qpc);
            FrameLatency latency;
            latency.sequence = sequence;
            latency.queue_ms = 0.0;
            latency.commit_ms = last_commit_ms_;
            latency.present_ms = MillisecondsToNextVblank(present_qpc.QuadPart);
            {
                std::lock_guard<std::mutex> lock(commit_mutex_);
                last_latency_ = latency;
            }

            last_frame_ms_ =
                std::chrono::duration<double, std::milli>(present_end - start).count();
            frame_count_.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        surface_state.surface->EndDraw();
        const auto surface_end = std::chrono::steady_clock::now();
        if (FAILED(result)) {
            return;
        }

        commit_pending_ = true;
        pending_surface_index_ = draw_index_;
        pending_sequence_ = sequence;
        pending_submit_time_ = std::chrono::steady_clock::now();
        LARGE_INTEGER submit_qpc{};
        QueryPerformanceCounter(&submit_qpc);
        pending_submit_qpc_ = submit_qpc.QuadPart;
        bool committed = false;
        if (force_commit || !throttle_commits_ || CommitDue()) {
            committed = Present();
        } else {
            ScheduleCommit();
        }
        const auto frame_end = std::chrono::steady_clock::now();

        FrameTimings timings;
        timings.sequence = sequence;
        timings.begin_draw_ms = std::chrono::duration<double, std::milli>(
            begin_end - begin_start).count();
        timings.draw_ms =
            std::chrono::duration<double, std::milli>(draw_end - begin_end).count();
        timings.end_draw_ms =
            std::chrono::duration<double, std::milli>(surface_end - draw_end).count();
        if (committed && !async_commit_) {
            timings.commit_ms = last_commit_ms_;
        }
        last_timings_ = timings;
        last_frame_ms_ =
            std::chrono::duration<double, std::milli>(frame_end - start).count();
        frame_count_.fetch_add(1, std::memory_order_relaxed);
    }

    bool Present() noexcept {
        if (!commit_pending_ || dcomp_device_ == nullptr) {
            return false;
        }

        if (async_commit_) {
            const int surface_index = pending_surface_index_;
            QueueCommit(
                surface_index,
                pending_sequence_,
                pending_submit_time_,
                pending_submit_qpc_);
            pending_surface_index_ = -1;
            commit_pending_ = false;
            last_commit_time_ = std::chrono::steady_clock::now();
            draw_index_ = (surface_index + 1) % surface_count_;
            CancelCommitTimer();
            return true;
        }

        const auto commit_start = std::chrono::steady_clock::now();
        const HRESULT result = dcomp_device_->Commit();
        const auto commit_end = std::chrono::steady_clock::now();
        last_commit_ms_ = std::chrono::duration<double, std::milli>(
            commit_end - commit_start).count();
        if (FAILED(result)) {
            ScheduleCommit();
            return false;
        }

        commit_pending_ = false;
        last_commit_time_ = commit_end;
        ++commit_count_;
        CancelCommitTimer();

        FrameLatency latency;
        latency.sequence = pending_sequence_;
        latency.commit_ms = last_commit_ms_;
        latency.present_ms = MillisecondsToNextVblank(pending_submit_qpc_);
        {
            std::lock_guard<std::mutex> lock(commit_mutex_);
            last_latency_ = latency;
        }
        return true;
    }

    void SetDiagnosticLoad(
        std::uint32_t rectangles,
        std::uint32_t text_runs,
        bool virtual_list) noexcept {
        diagnostic_rectangles_ = rectangles;
        diagnostic_text_runs_ = text_runs;
        diagnostic_virtual_list_ = virtual_list;
        full_redraw_required_ = true;
    }

    void SetDiagnosticScroll(float offset) noexcept {
        diagnostic_scroll_ = offset;
        full_redraw_required_ = true;
    }

    void SetDiagnosticTextList(
        std::span<const std::wstring> rows,
        float row_height,
        bool cache_layouts) noexcept {
        diagnostic_text_rows_.assign(rows.begin(), rows.end());
        diagnostic_row_height_ = row_height;
        diagnostic_cache_layouts_ = cache_layouts;
        diagnostic_layout_cache_.clear();
        diagnostic_rectangles_ = 0;
        diagnostic_text_runs_ = 0;
        diagnostic_virtual_list_ = true;
        full_redraw_required_ = true;
    }

    void SetDiagnosticAsyncSurfaces(int count) noexcept {
        if (count < kMinAsyncSurfaces || count > kMaxAsyncSurfaces) {
            forced_surface_count_ = 0;
        } else {
            forced_surface_count_ = count;
        }
        if (async_commit_) {
            const int content_width = PixelWidth();
            const int content_height = PixelHeight();
            Resize(content_width, content_height, true);
        }
    }

    int DiagnosticAsyncSurfaceCount() const noexcept {
        return surface_count_;
    }

    bool IsFlipModel() const noexcept {
        return flip_model_;
    }

    ComPtr<IDWriteTextLayout> DiagnosticLayout(std::uint32_t row) noexcept {
        if (row >= diagnostic_text_rows_.size()) {
            return nullptr;
        }
        if (diagnostic_cache_layouts_) {
            const auto found = diagnostic_layout_cache_.find(row);
            if (found != diagnostic_layout_cache_.end()) {
                return found->second;
            }
        }

        ComPtr<IDWriteTextLayout> layout;
        const auto& text = diagnostic_text_rows_[row];
        HRESULT result = dwrite_factory_->CreateTextLayout(
            text.c_str(),
            static_cast<UINT32>(text.size()),
            text_format_.Get(),
            800.0f,
            diagnostic_row_height_,
            layout.GetAddressOf());
        if (FAILED(result)) {
            return nullptr;
        }
        if (diagnostic_cache_layouts_) {
            if (diagnostic_layout_cache_.size() >= 512) {
                diagnostic_layout_cache_.erase(diagnostic_layout_cache_.begin());
            }
            diagnostic_layout_cache_.emplace(row, layout);
        }
        return layout;
    }

    void SetCommitThrottle(bool enabled) noexcept {
        throttle_commits_ = enabled;
        if (!enabled) {
            Present();
        }
    }

    void UpdateCommitInterval() noexcept {
        commit_interval_ms_ = QueryRefreshIntervalMilliseconds(hwnd_);
    }

    void SetAsyncCommit(bool enabled) noexcept {
        if (enabled == async_commit_) {
            return;
        }
        if (enabled && flip_model_) {
            return;
        }

        if (enabled) {
            async_commit_ = true;
            const int content_width = PixelWidth();
            const int content_height = PixelHeight();
            auto resized = Resize(content_width, content_height, true);
            if (!resized.ok() || surface_count_ < async_surface_count_) {
                async_commit_ = false;
                draw_index_ = 0;
                return;
            }
            if (!commit_thread_.joinable()) {
                commit_thread_ = std::thread([this] { CommitThreadMain(); });
            }
            draw_index_ = 1;
            full_redraw_required_ = true;
            return;
        }

        WaitForAllCommits();
        async_commit_ = false;
        draw_index_ = 0;
        const int content_width = PixelWidth();
        const int content_height = PixelHeight();
        Resize(content_width, content_height, true);
        full_redraw_required_ = true;
    }

    double LastFrameMilliseconds() const noexcept {
        return last_frame_ms_;
    }

    FrameTimings LastFrameTimings() const noexcept {
        return last_timings_;
    }

    FrameLatency LastPresentedFrame() const noexcept {
        std::lock_guard<std::mutex> lock(commit_mutex_);
        return last_latency_;
    }

    CommitQueueStats AsyncCommitStats() const noexcept {
        std::lock_guard<std::mutex> lock(commit_mutex_);
        return queue_stats_;
    }

    bool IsAsyncCommitEnabled() const noexcept {
        return async_commit_;
    }

    std::uint64_t FrameCount() const noexcept {
        return frame_count_.load(std::memory_order_relaxed);
    }

    std::uint64_t CommitCount() const noexcept {
        return commit_count_;
    }

private:
    WindowGraphics() = default;

    struct SurfaceState {
        ComPtr<IDCompositionVirtualSurface> surface;
        ComPtr<ID2D1Bitmap1> target_bitmap;
        int width = 0;
        int height = 0;
    };

    struct CommitJob {
        int surface_index = 0;
        std::uint64_t sequence = 0;
        std::chrono::steady_clock::time_point submit_time{};
        LONGLONG submit_qpc = 0;
    };

    void WaitForSurface(int index) noexcept {
        std::unique_lock<std::mutex> lock(commit_mutex_);
        if (!surface_busy_[index]) {
            return;
        }

        const auto wait_start = std::chrono::steady_clock::now();
        commit_cv_.wait(lock, [this, index] {
            return !surface_busy_[index];
        });
        ++queue_stats_.waits;
        queue_stats_.total_wait_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - wait_start).count();
    }

    void PrepareForResize() noexcept {
        DropQueuedCommits();
        std::unique_lock<std::mutex> lock(commit_mutex_);
        commit_cv_.wait(lock, [this] {
            return commit_in_flight_ < 0;
        });
    }

    HRESULT EnsureSurfaceSize(int index) noexcept {
        auto& state = surfaces_[index];
        if (state.surface == nullptr || (state.width == surface_width_ && state.height == surface_height_)) {
            return S_OK;
        }
        HRESULT result =
            state.surface->Resize(static_cast<UINT>(surface_width_), static_cast<UINT>(surface_height_));
        if (FAILED(result)) {
            return result;
        }
        state.width = surface_width_;
        state.height = surface_height_;
        state.target_bitmap.Reset();
        target_bound_index_ = -1;
        return S_OK;
    }

    void WaitForAllCommits() noexcept {
        std::unique_lock<std::mutex> lock(commit_mutex_);
        commit_cv_.wait(lock, [this] {
            for (int index = 0; index < kMaxAsyncSurfaces; ++index) {
                if (surface_busy_[index]) {
                    return false;
                }
            }
            return commit_in_flight_ < 0;
        });
    }

    void DropQueuedCommits() noexcept {
        std::unique_lock<std::mutex> lock(commit_mutex_);
        while (!commit_queue_.empty()) {
            const auto job = commit_queue_.front();
            commit_queue_.pop_front();
            surface_busy_[job.surface_index] = false;
            ++queue_stats_.dropped;
        }
        commit_cv_.notify_all();
    }

    void QueueCommit(
        int index,
        std::uint64_t sequence,
        std::chrono::steady_clock::time_point submit_time,
        LONGLONG submit_qpc) noexcept {
        std::lock_guard<std::mutex> lock(commit_mutex_);
        if (commit_queue_.size() >= kMaxQueuedCommits) {
            const auto dropped = commit_queue_.front();
            commit_queue_.pop_front();
            surface_busy_[dropped.surface_index] = false;
            ++queue_stats_.dropped;
        }
        surface_busy_[index] = true;
        commit_queue_.push_back(CommitJob{index, sequence, submit_time, submit_qpc});
        queue_stats_.submitted++;
        queue_stats_.max_depth = std::max(
            queue_stats_.max_depth,
            static_cast<std::uint32_t>(commit_queue_.size()));
        commit_cv_.notify_one();
    }

    void CommitThreadMain() noexcept {
        const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        for (;;) {
            std::unique_lock<std::mutex> lock(commit_mutex_);
            commit_cv_.wait(lock, [this] {
                return !commit_running_ || !commit_queue_.empty();
            });
            if (!commit_running_) {
                break;
            }

            const auto job = commit_queue_.front();
            commit_queue_.pop_front();
            commit_in_flight_ = job.surface_index;
            lock.unlock();

            const auto commit_start = std::chrono::steady_clock::now();
            HRESULT result = E_FAIL;
            if (visual_ != nullptr && surfaces_[job.surface_index].surface != nullptr) {
                result = visual_->SetContent(surfaces_[job.surface_index].surface.Get());
                if (SUCCEEDED(result)) {
                    result = dcomp_device_->Commit();
                }
            }
            const auto commit_end = std::chrono::steady_clock::now();

            FrameLatency latency;
            latency.sequence = job.sequence;
            latency.queue_ms = std::chrono::duration<double, std::milli>(
                commit_start - job.submit_time).count();
            latency.commit_ms = std::chrono::duration<double, std::milli>(
                commit_end - commit_start).count();
            latency.present_ms = MillisecondsToNextVblank(job.submit_qpc);

            lock.lock();
            last_latency_ = latency;
            last_commit_ms_ = latency.commit_ms;
            surface_busy_[job.surface_index] = false;
            commit_in_flight_ = -1;
            if (SUCCEEDED(result)) {
                commit_count_.fetch_add(1, std::memory_order_relaxed);
                ++queue_stats_.completed;
            }
            commit_cv_.notify_all();
        }
        if (com_result == S_OK || com_result == S_FALSE) {
            CoUninitialize();
        }
    }

    void StopCommitThread() noexcept {
        if (!commit_thread_.joinable()) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(commit_mutex_);
            commit_running_ = false;
        }
        commit_cv_.notify_all();
        commit_thread_.join();
    }

    bool CommitDue() const noexcept {
        if (!throttle_commits_) {
            return true;
        }
        if (last_commit_time_ == std::chrono::steady_clock::time_point{}) {
            return true;
        }
        const auto interval =
            std::chrono::duration<double, std::milli>(commit_interval_ms_ * 0.95);
        return std::chrono::steady_clock::now() - last_commit_time_ >= interval;
    }

    void ScheduleCommit() noexcept {
        if (commit_timer_active_ || hwnd_ == nullptr) {
            return;
        }
        const UINT interval = static_cast<UINT>(std::ceil(commit_interval_ms_));
        commit_timer_active_ =
            SetTimer(hwnd_, kCommitTimerId, interval, nullptr) != 0;
    }

    void CancelCommitTimer() noexcept {
        if (commit_timer_active_ && hwnd_ != nullptr) {
            KillTimer(hwnd_, kCommitTimerId);
        }
        commit_timer_active_ = false;
    }

    Result<void> Initialize(HWND hwnd, int width, int height) {
        hwnd_ = hwnd;
        UpdateCommitInterval();
        D3D_FEATURE_LEVEL feature_levels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        HRESULT result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            d3d_device_.GetAddressOf(),
            nullptr,
            nullptr);
        if (FAILED(result)) {
            result = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                feature_levels,
                ARRAYSIZE(feature_levels),
                D3D11_SDK_VERSION,
                d3d_device_.GetAddressOf(),
                nullptr,
                nullptr);
            if (FAILED(result)) {
                return Result<void>::Fail(
                    Error{ErrorCode::PlatformFailure, PlatformError("D3D11CreateDevice", result)});
            }
        }

        result = d3d_device_.As(&dxgi_device_);
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("QueryInterface IDXGIDevice", result)});
        }

        result = DCompositionCreateDevice(
            dxgi_device_.Get(),
            IID_PPV_ARGS(dcomp_device_.GetAddressOf()));
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("DCompositionCreateDevice", result)});
        }

        result = dcomp_device_->CreateTargetForHwnd(hwnd, TRUE, target_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateTargetForHwnd", result)});
        }

        result = dcomp_device_->CreateVisual(visual_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateVisual", result)});
        }

        result = target_->SetRoot(visual_.Get());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("SetRoot", result)});
        }

        D2D1_FACTORY_OPTIONS factory_options{};
        result = D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory1),
            &factory_options,
            reinterpret_cast<void**>(d2d_factory_.GetAddressOf()));
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("D2D1CreateFactory", result)});
        }

        result = d2d_factory_->CreateDevice(dxgi_device_.Get(), d2d_device_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateDevice", result)});
        }

        result = d2d_device_->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d_context_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateDeviceContext", result)});
        }
        d2d_context_->SetDpi(dpi_, dpi_);

        result = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf()));
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("DWriteCreateFactory", result)});
        }

        result = dwrite_factory_->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            26.0f,
            L"en-US",
            text_format_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateTextFormat", result)});
        }

        result = dwrite_factory_->CreateTextLayout(
            L"NopeUI",
            6,
            text_format_.Get(),
            800.0f,
            60.0f,
            text_layout_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateTextLayout", result)});
        }

        result = d2d_context_->CreateSolidColorBrush(
            D2D1::ColorF(0x1B1B1B), text_brush_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateSolidColorBrush", result)});
        }

        ComPtr<ID2D1BitmapRenderTarget> text_target;
        if (SUCCEEDED(d2d_context_->CreateCompatibleRenderTarget(
                D2D1::SizeF(360.0f, 64.0f), text_target.GetAddressOf()))) {
            ComPtr<ID2D1SolidColorBrush> cache_brush;
            if (SUCCEEDED(text_target->CreateSolidColorBrush(
                    D2D1::ColorF(0x1B1B1B), cache_brush.GetAddressOf()))) {
                text_target->BeginDraw();
                text_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
                text_target->DrawTextLayout(
                    D2D1::Point2F(0.0f, 0.0f),
                    text_layout_.Get(),
                    cache_brush.Get());
                if (SUCCEEDED(text_target->EndDraw())) {
                    text_target->GetBitmap(text_bitmap_.GetAddressOf());
                }
            }
        }

        result = d2d_context_->CreateSolidColorBrush(
            D2D1::ColorF(0x0067C0), accent_brush_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateSolidColorBrush", result)});
        }

        software_render_ = GetSystemMetrics(SM_REMOTESESSION) != 0;

        auto painter_result = graphics::ScenePainter::Create(
            d2d_context_.Get(),
            palette_);
        if (!painter_result.ok()) {
            return Result<void>::Fail(painter_result.error());
        }
        scene_painter_ = std::move(painter_result.value());

        auto surface_result = Resize(width, height);
        if (!surface_result.ok()) {
            return surface_result;
        }

        Render(nullptr, true);
        SetAsyncCommit(true);
        return Result<void>::Ok();
    }

    void ReleaseSurface() noexcept {
        CancelCommitTimer();
        swap_state_.target_bitmap.Reset();
        swap_chain_.Reset();
        for (auto& state : surfaces_) {
            state.target_bitmap.Reset();
            state.surface.Reset();
        }
        surface_count_ = 0;
        target_bound_index_ = -1;
        surface_width_ = 0;
        surface_height_ = 0;
        content_width_ = 0;
        content_height_ = 0;
    }

    ComPtr<ID3D11Device> d3d_device_;
    ComPtr<IDXGIDevice> dxgi_device_;
    ComPtr<IDCompositionDevice> dcomp_device_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<IDCompositionVisual> visual_;
    SurfaceState surfaces_[kMaxAsyncSurfaces];
    ComPtr<IDXGISwapChain1> swap_chain_;
    SurfaceState swap_state_;
    bool flip_model_ = false;
    ComPtr<ID2D1Factory1> d2d_factory_;
    ComPtr<ID2D1Device> d2d_device_;
    ComPtr<ID2D1DeviceContext> d2d_context_;
    std::unique_ptr<graphics::ScenePainter> scene_painter_;
    graphics::ScenePalette palette_ = graphics::LightPalette();
    std::unique_ptr<graphics::OffscreenSurface> software_surface_;
    std::unique_ptr<graphics::ScenePainter> software_painter_;
    ComPtr<ID2D1Bitmap1> software_bitmap_;
    Element* content_ = nullptr;
    bool software_render_ = false;
    ComPtr<IDWriteFactory> dwrite_factory_;
    ComPtr<IDWriteTextFormat> text_format_;
    ComPtr<IDWriteTextLayout> text_layout_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
    ComPtr<ID2D1Bitmap> text_bitmap_;
    ComPtr<ID2D1SolidColorBrush> accent_brush_;
    std::uint32_t diagnostic_rectangles_ = 0;
    std::uint32_t diagnostic_text_runs_ = 0;
    bool diagnostic_virtual_list_ = false;
    float diagnostic_scroll_ = 0.0f;
    std::vector<std::wstring> diagnostic_text_rows_;
    float diagnostic_row_height_ = 32.0f;
    bool diagnostic_cache_layouts_ = false;
    std::unordered_map<std::uint32_t, ComPtr<IDWriteTextLayout>> diagnostic_layout_cache_;
    bool device_lost_ = false;
    HWND hwnd_ = nullptr;
    int surface_width_ = 0;
    int surface_height_ = 0;
    int content_width_ = 0;
    int content_height_ = 0;
    int pixel_width_ = 0;
    int pixel_height_ = 0;
    float dpi_ = 96.0f;
    int surface_count_ = 0;
    int async_surface_count_ = kMinAsyncSurfaces;
    int forced_surface_count_ = 0;
    int draw_index_ = 0;
    int pending_surface_index_ = -1;
    int target_bound_index_ = -1;
    std::uint64_t frame_sequence_ = 0;
    std::uint64_t pending_sequence_ = 0;
    std::chrono::steady_clock::time_point pending_submit_time_{};
    LONGLONG pending_submit_qpc_ = 0;
    double commit_interval_ms_ = 1000.0 / 120.0;
    double last_frame_ms_ = 0.0;
    double last_commit_ms_ = 0.0;
    FrameTimings last_timings_{};
    FrameLatency last_latency_{};
    CommitQueueStats queue_stats_{};
    std::chrono::steady_clock::time_point last_commit_time_{};
    std::atomic<std::uint64_t> frame_count_{0};
    std::atomic<std::uint64_t> commit_count_{0};
    bool throttle_commits_ = true;
    bool commit_pending_ = false;
    bool commit_timer_active_ = false;
    bool async_commit_ = false;
    bool commit_running_ = true;
    bool full_redraw_required_ = false;
    bool surface_busy_[kMaxAsyncSurfaces] = {};
    int commit_in_flight_ = -1;
    std::deque<CommitJob> commit_queue_;
    std::thread commit_thread_;
    mutable std::mutex commit_mutex_;
    std::condition_variable commit_cv_;
};

class OverlayRoot final : public Element {
public:
    static Ref<OverlayRoot> Create() noexcept {
        return Ref<OverlayRoot>(new OverlayRoot());
    }

    void SetPrimaryContent(Ref<Element> content) noexcept {
        if (content_) {
            content_->SetParent(nullptr);
        }
        content_ = std::move(content);
        if (content_) {
            content_->SetParent(this);
        }
        InvalidateLayout();
    }

    void SetPopup(Ref<Element> popup, Point anchor) noexcept {
        ++generation_;
        if (popup_) {
            popup_->SetParent(nullptr);
        }
        popup_ = std::move(popup);
        anchor_ = anchor;
        if (popup_) {
            popup_->SetParent(this);
        }
        InvalidateLayout();
    }

    void ClearPopup() noexcept {
        ++generation_;
        if (popup_) {
            popup_->SetParent(nullptr);
        }
        popup_.Reset();
        popup_bounds_ = Rect{};
        InvalidateLayout();
    }

    std::uint64_t Generation() const noexcept {
        return generation_;
    }

    Element* Popup() const noexcept {
        return popup_.Get();
    }

    bool HasPopup() const noexcept {
        return static_cast<bool>(popup_);
    }

    bool PopupContains(Point point) const noexcept {
        return popup_ && !popup_bounds_.IsEmpty() &&
               point.x >= popup_bounds_.x && point.x < popup_bounds_.Right() &&
               point.y >= popup_bounds_.y && point.y < popup_bounds_.Bottom();
    }

    const Rect& PopupBounds() const noexcept {
        return popup_bounds_;
    }

    void GatherFocusables(std::vector<Element*>& elements) noexcept {
        CollectFocusable(elements);
    }

protected:
    Size MeasureOverride(Size available) noexcept override {
        if (content_) {
            content_->Measure(available);
        }
        if (popup_) {
            popup_->Measure(Size{available.width, available.height});
        }
        return available;
    }

    Size ArrangeOverride(Size final_size) noexcept override {
        if (content_) {
            content_->Arrange(Rect{0.0f, 0.0f, final_size.width, final_size.height});
        }
        if (popup_) {
            const Size desired = popup_->DesiredSize();
            const float width = std::min(desired.width, final_size.width);
            const float height = std::min(desired.height, final_size.height);
            const float max_x = std::max(0.0f, final_size.width - width);
            const float max_y = std::max(0.0f, final_size.height - height);
            const float x = std::min(std::max(anchor_.x, 0.0f), max_x);
            const float y = std::min(std::max(anchor_.y, 0.0f), max_y);
            popup_bounds_ = Rect{x, y, width, height};
            popup_->Arrange(popup_bounds_);
        }
        return final_size;
    }

    void PaintContent(ID2D1RenderTarget* target) noexcept override {
        if (content_) {
            content_->Paint(target);
        }
        if (popup_) {
            popup_->Paint(target);
        }
    }

    Element* HitTestOverride(Point point) noexcept override {
        if (popup_ && PopupContains(point)) {
            Element* hit = popup_->HitTest(point);
            if (hit != nullptr) {
                return hit;
            }
        }
        if (content_) {
            return content_->HitTest(point);
        }
        return nullptr;
    }

    void CollectFocusable(std::vector<Element*>& elements) noexcept override {
        if (!IsEnabled()) {
            return;
        }
        if (popup_) {
            popup_->CollectFocusable(elements);
        } else if (content_) {
            content_->CollectFocusable(elements);
        }
    }

    void CollectChildren(std::vector<Element*>& elements) noexcept override {
        if (content_) {
            elements.push_back(content_.Get());
        }
        if (popup_) {
            elements.push_back(popup_.Get());
        }
    }

private:
    OverlayRoot() noexcept = default;

    Ref<Element> content_;
    Ref<Element> popup_;
    Point anchor_{};
    Rect popup_bounds_{};
    std::uint64_t generation_ = 0;
};

Window::Window() noexcept : theme_(SystemTheme()) {}

bool Window::EnsureWindowClass() noexcept {
    static std::once_flag once;
    static bool registered = false;

    std::call_once(once, [] {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.lpfnWndProc = &Window::WndProc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        window_class.lpszClassName = kWindowClassName;

        registered = RegisterClassExW(&window_class) != 0;
    });

    return registered;
}

Result<Ref<Window>> Window::Create(int width, int height, std::u16string_view title) {
    if (width <= 0 || height <= 0) {
        return Result<Ref<Window>>::Fail(
            Error{ErrorCode::InvalidArgument, "window size must be positive"});
    }

    EnableDpiAwareness();
    if (!EnsureWindowClass()) {
        return Result<Ref<Window>>::Fail(
            Error{ErrorCode::PlatformFailure, "RegisterClassExW failed with error " +
                                                  std::to_string(GetLastError())});
    }

    Ref<Window> window(new Window());
    const std::wstring native_title = ToWideString(title);
    const float initial_scale = DpiScaleFrom(GetDpiForSystem());
    const int pixel_width = static_cast<int>(std::lround(static_cast<double>(width) * initial_scale));
    const int pixel_height = static_cast<int>(std::lround(static_cast<double>(height) * initial_scale));

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        native_title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        pixel_width,
        pixel_height,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        window.Get());

    if (hwnd == nullptr) {
        return Result<Ref<Window>>::Fail(
            Error{ErrorCode::PlatformFailure, "CreateWindowExW failed with error " +
                                                  std::to_string(GetLastError())});
    }

    window->hwnd_ = hwnd;

    RECT client{};
    if (!GetClientRect(hwnd, &client)) {
        window->Close();
        return Result<Ref<Window>>::Fail(
            Error{ErrorCode::PlatformFailure, "GetClientRect failed with error " +
                                                  std::to_string(GetLastError())});
    }

    auto graphics = WindowGraphics::Create(
        hwnd, client.right - client.left, client.bottom - client.top);
    if (!graphics.ok()) {
        window->Close();
        return Result<Ref<Window>>::Fail(graphics.error());
    }

    const UINT dpi = QueryWindowDpi(hwnd);
    window->scale_ = DpiScaleFrom(dpi);
    window->graphics_ = std::move(graphics).value();
    window->graphics_->SetDpi(static_cast<float>(dpi));
    window->graphics_->SetPalette(PaletteFromTheme(window->theme_));
    window->UpdateClientMetrics(client.right - client.left, client.bottom - client.top);
    return Result<Ref<Window>>::Ok(std::move(window));
}

Window::~Window() {
    ReleaseAutomation(this);
    ReleaseGraphics();
    if (root_ != nullptr) {
        root_->Release();
        root_ = nullptr;
    }
    if (style_sheet_ != nullptr) {
        style_sheet_->Release();
        style_sheet_ = nullptr;
    }
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void Window::Show() noexcept {
    if (hwnd_ != nullptr) {
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        UpdateWindow(hwnd_);
        if (graphics_ != nullptr) {
            graphics_->UpdateCommitInterval();
            if (resize_pending_) {
                RenderPendingResize();
            } else {
                graphics_->Render(nullptr, true);
            }
        }
    }
}

void Window::Hide() noexcept {
    if (hwnd_ != nullptr) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void Window::Close() noexcept {
    ReleaseGraphics();
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
}

bool Window::IsVisible() const noexcept {
    return hwnd_ != nullptr && IsWindowVisible(hwnd_) != FALSE;
}

double Window::LastFrameMilliseconds() const noexcept {
    return graphics_ != nullptr ? graphics_->LastFrameMilliseconds() : 0.0;
}

FrameTimings Window::LastFrameTimings() const noexcept {
    return graphics_ != nullptr ? graphics_->LastFrameTimings() : FrameTimings{};
}

double Window::LastResizeMilliseconds() const noexcept {
    return last_resize_ms_;
}

FrameLatency Window::LastPresentedFrame() const noexcept {
    return graphics_ != nullptr ? graphics_->LastPresentedFrame() : FrameLatency{};
}

CommitQueueStats Window::AsyncCommitStats() const noexcept {
    return graphics_ != nullptr ? graphics_->AsyncCommitStats() : CommitQueueStats{};
}

bool Window::IsAsyncCommitEnabled() const noexcept {
    return graphics_ != nullptr && graphics_->IsAsyncCommitEnabled();
}

std::uint64_t Window::FrameCount() const noexcept {
    return graphics_ != nullptr ? graphics_->FrameCount() : 0;
}

std::uint64_t Window::CommitCount() const noexcept {
    return graphics_ != nullptr ? graphics_->CommitCount() : 0;
}

void Window::RenderFrame() noexcept {
    if (graphics_ == nullptr) {
        return;
    }
    if (graphics_->TakeDeviceLost()) {
        if (!recreate_pending_ && hwnd_ != nullptr) {
            recreate_pending_ = true;
            PostMessageW(hwnd_, kRecreateGraphicsMessage, 0, 0);
        }
        return;
    }

    if (resize_pending_) {
        RenderPendingResize();
        return;
    }

    graphics_->Render();
}

void Window::SetDiagnosticLoad(
    std::uint32_t rectangles,
    std::uint32_t text_runs,
    bool virtual_list) noexcept {
    if (graphics_ != nullptr) {
        graphics_->SetDiagnosticLoad(rectangles, text_runs, virtual_list);
    }
}

void Window::SetDiagnosticScroll(float offset) noexcept {
    if (graphics_ != nullptr) {
        graphics_->SetDiagnosticScroll(offset);
    }
}

void Window::SetDiagnosticTextList(
    std::span<const std::wstring> rows,
    float row_height,
    bool cache_layouts) noexcept {
    if (graphics_ != nullptr) {
        graphics_->SetDiagnosticTextList(rows, row_height, cache_layouts);
    }
}

void Window::SetDiagnosticAsyncSurfaces(int count) noexcept {
    if (graphics_ != nullptr) {
        graphics_->SetDiagnosticAsyncSurfaces(count);
    }
}

int Window::DiagnosticAsyncSurfaceCount() const noexcept {
    return graphics_ != nullptr ? graphics_->DiagnosticAsyncSurfaceCount() : 0;
}

void Window::SetDiagnosticFlipModel(bool enabled) noexcept {
    if (graphics_ != nullptr) {
        graphics_->SetFlipModel(enabled);
    }
}

bool Window::IsDiagnosticFlipModel() const noexcept {
    return graphics_ != nullptr && graphics_->IsFlipModel();
}

void Window::RenderFrame(const Rect& dirty_rect) noexcept {
    if (graphics_ == nullptr || dirty_rect.IsEmpty()) {
        return;
    }

    if (resize_pending_) {
        RenderPendingResize();
        return;
    }

    RECT native_rect{};
    native_rect.left = static_cast<LONG>(dirty_rect.x);
    native_rect.top = static_cast<LONG>(dirty_rect.y);
    native_rect.right = static_cast<LONG>(dirty_rect.Right());
    native_rect.bottom = static_cast<LONG>(dirty_rect.Bottom());
    graphics_->Render(&native_rect);
}

void Window::Present() noexcept {
    if (graphics_ != nullptr) {
        graphics_->Present();
    }
}

void Window::SetCommitThrottle(bool enabled) noexcept {
    if (graphics_ != nullptr) {
        graphics_->SetCommitThrottle(enabled);
    }
}

void Window::SetAsyncCommit(bool enabled) noexcept {
    if (graphics_ != nullptr) {
        graphics_->SetAsyncCommit(enabled);
    }
}

void Window::SetSoftwareRender(bool enabled) noexcept {
    if (graphics_ != nullptr) {
        graphics_->SetSoftwareRender(enabled);
    }
}

bool Window::IsSoftwareRender() const noexcept {
    return graphics_ != nullptr && graphics_->IsSoftwareRender();
}

void Window::SetContent(Ref<Element> content) noexcept {
    if (root_ != nullptr) {
        root_->ClearPopup();
        root_->SetPrimaryContent(Ref<Element>());
    } else if (content_) {
        content_->SetParent(nullptr);
    }
    content_ = std::move(content);
    hovered_ = nullptr;
    pressed_ = nullptr;
    focused_ = nullptr;
    previous_focus_ = nullptr;
    popup_close_pending_ = false;
    popup_is_tooltip_ = false;
    tracking_mouse_ = false;

    if (content_) {
        if (root_ == nullptr) {
            Ref<OverlayRoot> created = OverlayRoot::Create();
            root_ = created.Get();
            root_->AddRef();
            root_->SetTheme(&theme_);
            root_->SetInvalidateHandler([this] { RenderFrame(); });
            root_->SetRelayoutHandler([this] {
                LayoutContent(client_width_, client_height_);
                RenderFrame();
            });
            root_->SetPopupHandler(
                [this](Ref<Element> popup, Point anchor) {
                    ShowPopup(std::move(popup), anchor);
                },
                [this] { ClosePopup(); });
        }
        root_->SetPrimaryContent(content_);
        content_->RefreshThemeResources();
    } else if (root_ != nullptr) {
        root_->Release();
        root_ = nullptr;
    }

    ApplyStyleSheet();
    if (graphics_ != nullptr) {
        graphics_->SetContent(root_);
    }
    LayoutContent(client_width_, client_height_);
    RenderFrame();
}

Element* Window::Content() const noexcept {
    return content_.Get();
}

void Window::ShowPopup(Ref<Element> popup, Point anchor) noexcept {
    if (!popup || !content_) {
        return;
    }
    if (root_ == nullptr) {
        Ref<OverlayRoot> created = OverlayRoot::Create();
        root_ = created.Get();
        root_->AddRef();
        root_->SetTheme(&theme_);
        root_->SetInvalidateHandler([this] { RenderFrame(); });
        root_->SetRelayoutHandler([this] {
            LayoutContent(client_width_, client_height_);
            RenderFrame();
        });
        root_->SetPopupHandler(
            [this](Ref<Element> popup, Point anchor) {
                ShowPopup(std::move(popup), anchor);
            },
            [this] { ClosePopup(); });
        root_->SetPrimaryContent(content_);
        if (graphics_ != nullptr) {
            graphics_->SetContent(root_);
        }
    }
    if (root_->HasPopup()) {
        root_->NotifyPopupClosed();
        root_->ClearPopup();
    } else {
        previous_focus_ = focused_;
    }
    popup_close_pending_ = false;
    popup_is_tooltip_ = false;
    if (tooltip_timer_active_ && hwnd_ != nullptr) {
        KillTimer(hwnd_, kToolTipTimerId);
        tooltip_timer_active_ = false;
    }
    root_->SetPopup(std::move(popup), anchor);
    LayoutContent(client_width_, client_height_);
    if (root_->HasPopup() && root_->Popup() != nullptr && root_->Popup()->IsFocusable()) {
        FocusElement(root_->Popup());
    }
    RenderFrame();
}

void Window::ClosePopup() noexcept {
    if (!root_ || !root_->HasPopup() || popup_close_pending_) {
        return;
    }
    popup_close_pending_ = true;
    pending_close_generation_ = root_->Generation();
    if (hwnd_ != nullptr) {
        PostMessageW(hwnd_, kClosePopupMessage, 0, 0);
    }
}

bool Window::HasPopup() const noexcept {
    return root_ && root_->HasPopup();
}

void Window::SetStyleSheet(Ref<StyleSheet> style_sheet) noexcept {
    if (style_sheet_ != nullptr) {
        style_sheet_->Release();
    }
    style_sheet_ = style_sheet.Get();
    if (style_sheet_ != nullptr) {
        style_sheet_->AddRef();
    }
    ApplyStyleSheet();
    LayoutContent(client_width_, client_height_);
    RenderFrame();
}

StyleSheet* Window::GetStyleSheet() const noexcept {
    return style_sheet_;
}

void Window::ApplyStyleSheet() noexcept {
    if (content_ && style_sheet_) {
        style_sheet_->ApplyTo(*content_);
    }
}

void Window::SetTheme(Theme theme) noexcept {
    theme_ = std::move(theme);
    if (root_) {
        root_->SetTheme(&theme_);
        root_->RefreshThemeResources();
        LayoutContent(client_width_, client_height_);
    }
    if (graphics_ != nullptr) {
        graphics_->SetPalette(PaletteFromTheme(theme_));
    }
    RenderFrame();
}

const Theme& Window::GetTheme() const noexcept {
    return theme_;
}

void Window::LayoutContent(int width, int height) noexcept {
    if (width <= 0 || height <= 0) {
        return;
    }
    const Size available{
        static_cast<float>(width),
        static_cast<float>(height)};
    if (root_) {
        root_->Measure(available);
        root_->Arrange(Rect{
            0.0f,
            0.0f,
            static_cast<float>(width),
            static_cast<float>(height)});
        return;
    }
    if (!content_) {
        return;
    }
    content_->Measure(available);
    content_->Arrange(Rect{
        0.0f,
        0.0f,
        static_cast<float>(width),
        static_cast<float>(height)});
}

void Window::OnMouseMove(Point point) noexcept {
    pointer_ = point;
    if (!root_ || graphics_ == nullptr) {
        return;
    }
    if (!tracking_mouse_ && hwnd_ != nullptr) {
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd_;
        TrackMouseEvent(&track);
        tracking_mouse_ = true;
    }

    if (root_->HasPopup() && !root_->PopupContains(point)) {
        if (popup_is_tooltip_) {
            ClosePopup();
        }
        if (hovered_ != nullptr) {
            hovered_->OnPointerLeave();
            hovered_ = nullptr;
            UpdateToolTipTimer();
            RenderFrame();
        }
        return;
    }

    Element* hit = root_->HitTest(point);
    if (pressed_ != nullptr) {
        // Pointer capture: while a button or scroll thumb is held, moves go to it.
        pressed_->OnPointerMove(point);
        return;
    }
    bool changed = false;
    if (hit != hovered_) {
        if (hovered_ != nullptr) {
            hovered_->OnPointerLeave();
        }
        hovered_ = hit;
        if (hit != nullptr) {
            hit->OnPointerEnter();
        }
        changed = true;
        UpdateToolTipTimer();
    }
    if (hovered_ != nullptr) {
        hovered_->OnPointerMove(point);
    }
    if (changed) {
        RenderFrame();
    }
}

void Window::OnMouseDown(Point point) noexcept {
    pointer_ = point;
    if (!root_) {
        return;
    }
    if (root_->HasPopup()) {
        if (popup_is_tooltip_) {
            root_->ClearPopup();
            popup_is_tooltip_ = false;
            popup_close_pending_ = false;
        } else if (!root_->PopupContains(point)) {
            ClosePopup();
            return;
        }
    }
    Element* hit = root_->HitTest(point);
    FocusElement(hit != nullptr && hit->IsFocusable() ? hit : nullptr);
    pressed_ = hit;
    if (hit != nullptr) {
        hit->OnPointerDown(point);
    }
    UpdateToolTipTimer();
    RenderFrame();
}

void Window::OnMouseUp(Point point) noexcept {
    Element* pressed = pressed_;
    pressed_ = nullptr;
    if (pressed != nullptr) {
        pressed->OnPointerUp(point);
        if (pressed->ClosePopupOnClick()) {
            ClosePopup();
        }
        RenderFrame();
    }
}

void Window::OnRightMouseDown(Point point) noexcept {
    pointer_ = point;
    if (!root_) {
        return;
    }
    if (root_->HasPopup()) {
        if (popup_is_tooltip_) {
            root_->ClearPopup();
            popup_is_tooltip_ = false;
            popup_close_pending_ = false;
        } else {
            ClosePopup();
            return;
        }
    }
    Element* hit = root_->HitTest(point);
    while (hit != nullptr) {
        if (hit->GetContextMenu() != nullptr) {
            Element* menu = hit->GetContextMenu();
            menu->AddRef();
            ShowPopup(Ref<Element>(menu), point);
            popup_is_tooltip_ = false;
            return;
        }
        hit = hit->Parent();
    }
}

void Window::OnMouseLeave() noexcept {
    tracking_mouse_ = false;
    if (popup_is_tooltip_) {
        ClosePopup();
    }
    if (hovered_ == nullptr) {
        return;
    }
    hovered_->OnPointerLeave();
    hovered_ = nullptr;
    CancelPointerPress();
    UpdateToolTipTimer();
    RenderFrame();
}

void Window::CancelPointerPress() noexcept {
    if (pressed_ == nullptr) {
        return;
    }
    Element* pressed = pressed_;
    pressed_ = nullptr;
    // A point far outside every element cancels the press instead of
    // activating it; controls that only track drag state end it regardless.
    pressed->OnPointerUp(Point{-100000.0f, -100000.0f});
    RenderFrame();
}

void Window::OnMouseWheel(Point point, int delta) noexcept {
    if (!root_ || delta == 0) {
        return;
    }
    if (root_->HasPopup() && !root_->PopupContains(point)) {
        ClosePopup();
        return;
    }
    Element* hit = root_->HitTest(point);
    if (hit != nullptr) {
        // A handler that consumes the wheel is expected to invalidate itself.
        hit->BubbleWheel(delta);
    }
}

void Window::OnToolTipTimer() noexcept {
    tooltip_timer_active_ = false;
    if (hwnd_ != nullptr) {
        KillTimer(hwnd_, kToolTipTimerId);
    }
    if (root_ == nullptr || root_->HasPopup() || hovered_ == nullptr) {
        return;
    }
    const std::wstring text = ToolTipFor(hovered_);
    if (text.empty()) {
        return;
    }
    auto tip = ToolTip::Create(text);
    const Rect bounds = hovered_->Bounds();
    ShowPopup(std::move(tip), Point{bounds.x + 4.0f, bounds.Bottom() + 6.0f});
    popup_is_tooltip_ = true;
}

void Window::UpdateToolTipTimer() noexcept {
    if (hwnd_ == nullptr) {
        return;
    }
    if (tooltip_timer_active_) {
        KillTimer(hwnd_, kToolTipTimerId);
        tooltip_timer_active_ = false;
    }
    if (hovered_ == nullptr || root_ == nullptr || root_->HasPopup()) {
        return;
    }
    if (ToolTipFor(hovered_).empty()) {
        return;
    }
    SetTimer(hwnd_, kToolTipTimerId, kToolTipDelayMs, nullptr);
    tooltip_timer_active_ = true;
}

std::wstring Window::ToolTipFor(const Element* element) const {
    const Element* node = element;
    while (node != nullptr) {
        const std::wstring text = ToolTipService::GetToolTip(node);
        if (!text.empty()) {
            return text;
        }
        node = node->Parent();
    }
    return {};
}

void Window::FocusElement(Element* element) noexcept {
    if (focused_ == element) {
        return;
    }
    if (focused_ != nullptr) {
        focused_->SetFocused(false);
    }
    focused_ = element;
    if (focused_ != nullptr) {
        focused_->SetFocused(true);
        Element* node = focused_->Parent();
        while (node != nullptr) {
            node->BringChildIntoView(focused_);
            node = node->Parent();
        }
    }
    NotifyAutomationFocus(this, focused_);
}

void Window::FocusFromAccessibility(Element* element) noexcept {
    if (hwnd_ != nullptr) {
        ::SetFocus(hwnd_);
    }
    FocusElement(element);
    RenderFrame();
}

void Window::MoveFocus(bool forward) noexcept {
    if (!root_) {
        return;
    }
    std::vector<Element*> focusables;
    root_->GatherFocusables(focusables);
    if (focusables.empty()) {
        return;
    }

    std::size_t index = focusables.size();
    for (std::size_t i = 0; i < focusables.size(); ++i) {
        if (focusables[i] == focused_) {
            index = i;
            break;
        }
    }
    std::size_t next = 0;
    if (forward) {
        next = index < focusables.size() ? (index + 1) % focusables.size() : 0;
    } else {
        next = index == 0 ? focusables.size() - 1
                          : (index < focusables.size() ? index - 1 : focusables.size() - 1);
    }
    FocusElement(focusables[next]);
    RenderFrame();
}

void Window::ClearFocus() noexcept {
    if (focused_ == nullptr) {
        return;
    }
    focused_->SetFocused(false);
    focused_ = nullptr;
    RenderFrame();
}

bool Window::OnKeyDown(unsigned int key) noexcept {
    if (key == VK_ESCAPE && root_ && root_->HasPopup()) {
        ClosePopup();
        return true;
    }
    Element* node = focused_;
    while (node != nullptr) {
        if (node->OnKeyDown(key)) {
            return true;
        }
        node = node->Parent();
    }
    return false;
}

bool Window::OnKeyUp(unsigned int key) noexcept {
    Element* node = focused_;
    while (node != nullptr) {
        if (node->OnKeyUp(key)) {
            return true;
        }
        node = node->Parent();
    }
    return false;
}

bool Window::OnChar(wchar_t character) noexcept {
    if (focused_ == nullptr) {
        return false;
    }
    return focused_->OnChar(character);
}

void Window::OnImeStart() noexcept {
    if (focused_ == nullptr) {
        return;
    }
    focused_->OnImeStart();
    HIMC context = ImmGetContext(hwnd_);
    if (context == nullptr) {
        return;
    }
    COMPOSITIONFORM form{};
    form.dwStyle = CFS_POINT;
    const Point caret = focused_->ImeCaretPoint();
    form.ptCurrentPos.x = static_cast<LONG>(caret.x);
    form.ptCurrentPos.y = static_cast<LONG>(caret.y);
    ImmSetCompositionWindow(context, &form);
    ImmReleaseContext(hwnd_, context);
}

void Window::OnImeComposition(std::wstring_view text) noexcept {
    if (focused_ != nullptr) {
        focused_->OnImeComposition(text);
    }
}

void Window::OnImeResult(std::wstring_view text) noexcept {
    if (focused_ != nullptr) {
        focused_->OnImeResult(text);
    }
}

void Window::OnImeEnd() noexcept {
    if (focused_ != nullptr) {
        focused_->OnImeEnd();
    }
}

HWND Window::Handle() const noexcept {
    return hwnd_;
}

void Window::UpdateClientMetrics(int pixel_width, int pixel_height) noexcept {
    if (pixel_width <= 0 || pixel_height <= 0) {
        return;
    }
    client_pixel_width_ = pixel_width;
    client_pixel_height_ = pixel_height;
    const UINT dpi = QueryWindowDpi(hwnd_);
    scale_ = DpiScaleFrom(dpi);
    if (graphics_ != nullptr) {
        graphics_->SetDpi(static_cast<float>(dpi));
    }
    client_width_ = std::max(1, static_cast<int>(
        std::lround(static_cast<double>(pixel_width) / static_cast<double>(scale_))));
    client_height_ = std::max(1, static_cast<int>(
        std::lround(static_cast<double>(pixel_height) / static_cast<double>(scale_))));
}

Point Window::ToClientDip(int pixel_x, int pixel_y) const noexcept {
    return Point{
        static_cast<float>(pixel_x) / scale_,
        static_cast<float>(pixel_y) / scale_};
}

void Window::OnSize(int width, int height) noexcept {
    if (graphics_ == nullptr || width <= 0 || height <= 0) {
        return;
    }

    if (width == client_pixel_width_ && height == client_pixel_height_ &&
        !resize_pending_) {
        return;
    }

    pending_width_ = width;
    pending_height_ = height;
    resize_pending_ = true;
    InvalidateRect(hwnd_, nullptr, FALSE);
    if (live_resizing_ || GetCapture() == hwnd_) {
        RenderPendingResize();
    }
}

void Window::OnSizing(const RECT& proposed) noexcept {
    if (graphics_ == nullptr || hwnd_ == nullptr) {
        return;
    }

    RECT window_rect{};
    RECT client_rect{};
    if (GetWindowRect(hwnd_, &window_rect) == FALSE ||
        GetClientRect(hwnd_, &client_rect) == FALSE) {
        return;
    }
    const int border_width = (window_rect.right - window_rect.left) -
                             (client_rect.right - client_rect.left);
    const int border_height = (window_rect.bottom - window_rect.top) -
                              (client_rect.bottom - client_rect.top);
    const int width = proposed.right - proposed.left - border_width;
    const int height = proposed.bottom - proposed.top - border_height;
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width == client_pixel_width_ && height == client_pixel_height_ &&
        !resize_pending_) {
        return;
    }

    pending_width_ = width;
    pending_height_ = height;
    resize_pending_ = true;
    RenderPendingResize();
}

void Window::BeginLiveResize() noexcept {
    live_resizing_ = true;
    if (hwnd_ != nullptr) {
        SetTimer(hwnd_, kLiveResizeTimerId, kLiveResizeIntervalMs, nullptr);
    }
}

void Window::EndLiveResize() noexcept {
    if (hwnd_ != nullptr) {
        KillTimer(hwnd_, kLiveResizeTimerId);
    }
    RenderLiveResize();
    live_resizing_ = false;
}

void Window::RenderLiveResize() noexcept {
    if ((!live_resizing_ && GetCapture() != hwnd_) || graphics_ == nullptr || hwnd_ == nullptr) {
        return;
    }

    RECT client{};
    if (GetClientRect(hwnd_, &client) == FALSE) {
        return;
    }
    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width == client_pixel_width_ && height == client_pixel_height_ &&
        !resize_pending_) {
        return;
    }

    pending_width_ = width;
    pending_height_ = height;
    resize_pending_ = true;
    RenderPendingResize();
}

void Window::RenderPendingResize() noexcept {
    if (!resize_pending_ || graphics_ == nullptr || hwnd_ == nullptr) {
        return;
    }

    resize_pending_ = false;
    const int width = pending_width_;
    const int height = pending_height_;
    UpdateClientMetrics(width, height);
    const auto resize_start = std::chrono::steady_clock::now();
    auto resized = graphics_->Resize(width, height);
    last_resize_ms_ = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - resize_start).count();
    if (!resized.ok()) {
        resize_pending_ = true;
        return;
    }

    LayoutContent(client_width_, client_height_);
    graphics_->Render(nullptr, !graphics_->IsAsyncCommitEnabled());
    ScheduleResizeSettle();
}

void Window::ScheduleResizeSettle() noexcept {
    if (hwnd_ != nullptr) {
        SetTimer(hwnd_, kResizeSettleTimerId, kResizeSettleIntervalMs, nullptr);
    }
}

void Window::CompactResize() noexcept {
    if (hwnd_ != nullptr) {
        KillTimer(hwnd_, kResizeSettleTimerId);
    }
    if (live_resizing_ || graphics_ == nullptr || client_pixel_width_ <= 0 ||
        client_pixel_height_ <= 0) {
        ScheduleResizeSettle();
        return;
    }

    const auto resize_start = std::chrono::steady_clock::now();
    auto resized = graphics_->Resize(client_pixel_width_, client_pixel_height_, true);
    last_resize_ms_ = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - resize_start).count();
    if (resized.ok()) {
        graphics_->Render(nullptr, true);
    }
}

void Window::RecreateGraphics() noexcept {
    recreate_pending_ = false;
    if (hwnd_ == nullptr || root_ == nullptr) {
        return;
    }
    RECT client{};
    if (GetClientRect(hwnd_, &client) == FALSE) {
        return;
    }
    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
    if (width <= 0 || height <= 0) {
        return;
    }
    auto graphics = WindowGraphics::Create(hwnd_, width, height);
    if (!graphics.ok()) {
        return;
    }
    const UINT dpi = QueryWindowDpi(hwnd_);
    scale_ = DpiScaleFrom(dpi);
    graphics_ = std::move(graphics).value();
    graphics_->SetDpi(static_cast<float>(dpi));
    graphics_->SetPalette(PaletteFromTheme(theme_));
    graphics_->SetContent(root_);
    UpdateClientMetrics(width, height);
    root_->SetInvalidateHandler([this] { RenderFrame(); });
    root_->SetRelayoutHandler([this] {
        if (root_ != nullptr) {
            LayoutContent(client_width_, client_height_);
            RenderFrame();
        }
    });
    LayoutContent(client_width_, client_height_);
    RenderFrame();
}

void Window::ReleaseGraphics() noexcept {
    if (hwnd_ != nullptr) {
        KillTimer(hwnd_, kCommitTimerId);
        KillTimer(hwnd_, kLiveResizeTimerId);
        KillTimer(hwnd_, kResizeSettleTimerId);
        KillTimer(hwnd_, kToolTipTimerId);
    }
    tooltip_timer_active_ = false;
    if (root_) {
        root_->SetInvalidateHandler(nullptr);
        root_->SetRelayoutHandler(nullptr);
        root_->SetPopupHandler(nullptr, nullptr);
    }
    live_resizing_ = false;
    graphics_.reset();
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    Window* window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        window = static_cast<Window*>(create_struct->lpCreateParams);
        if (window != nullptr) {
            window->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }
    }

    switch (message) {
        case kRecreateGraphicsMessage:
            if (window != nullptr) {
                window->RecreateGraphics();
            }
            return 0;

        case kClosePopupMessage:
            if (window != nullptr && window->root_) {
                const bool close =
                    window->popup_close_pending_ && window->root_->HasPopup() &&
                    window->pending_close_generation_ == window->root_->Generation();
                window->popup_close_pending_ = false;
                if (!close) {
                    return 0;
                }
                const bool was_tooltip = window->popup_is_tooltip_;
                window->popup_is_tooltip_ = false;
                Element* focused = window->focused_;
                bool focus_in_content = false;
                Element* node = focused;
                while (node != nullptr) {
                    if (node == window->content_.Get()) {
                        focus_in_content = true;
                        break;
                    }
                    node = node->Parent();
                }
                if (!focus_in_content) {
                    window->ClearFocus();
                }
                window->root_->ClearPopup();
                window->root_->NotifyPopupClosed();
                if (!was_tooltip && window->previous_focus_ != nullptr &&
                    window->previous_focus_ != focused) {
                    window->FocusElement(window->previous_focus_);
                }
                window->previous_focus_ = nullptr;
                window->LayoutContent(window->client_width_, window->client_height_);
                window->RenderFrame();
            }
            return 0;

        case WM_CAPTURECHANGED:
            if (window != nullptr) {
                window->EndLiveResize();
                window->CancelPointerPress();
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (window != nullptr) {
                window->ReleaseGraphics();
                window->hwnd_ = nullptr;
            }
            PostQuitMessage(0);
            return 0;

        case WM_SIZING:
            if (window != nullptr && lparam != 0) {
                window->OnSizing(*reinterpret_cast<const RECT*>(lparam));
            }
            return TRUE;

        case WM_NCLBUTTONDOWN:
            if (window != nullptr && wparam >= HTLEFT && wparam <= HTBOTTOMRIGHT) {
                window->BeginLiveResize();
            }
            break;

        case WM_ENTERSIZEMOVE:
            if (window != nullptr) {
                window->BeginLiveResize();
            }
            return 0;

        case WM_EXITSIZEMOVE:
            if (window != nullptr) {
                window->EndLiveResize();
            }
            return 0;

        case WM_WINDOWPOSCHANGED:
            DefWindowProcW(hwnd, message, wparam, lparam);
            if (window != nullptr) {
                window->RenderLiveResize();
            }
            return 0;

        case WM_SIZE:
            if (window != nullptr && wparam != SIZE_MINIMIZED) {
                window->OnSize(LOWORD(lparam), HIWORD(lparam));
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
            if (window != nullptr) {
                window->OnMouseMove(window->ToClientDip(
                    GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
            }
            return 0;

        case WM_MOUSELEAVE:
            if (window != nullptr) {
                window->OnMouseLeave();
            }
            return 0;

        case WM_MOUSEWHEEL: {
            if (window != nullptr) {
                POINT cursor{
                    GET_X_LPARAM(lparam),
                    GET_Y_LPARAM(lparam)};
                ScreenToClient(hwnd, &cursor);
                window->OnMouseWheel(
                    window->ToClientDip(cursor.x, cursor.y),
                    GET_WHEEL_DELTA_WPARAM(wparam));
            }
            return 0;
        }

        case WM_RBUTTONDOWN:
            if (window != nullptr) {
                window->OnRightMouseDown(window->ToClientDip(
                    GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
            }
            return 0;

        case WM_RBUTTONUP:
            return 0;

        case WM_LBUTTONDOWN:
            if (window != nullptr) {
                window->OnMouseDown(window->ToClientDip(
                    GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
                SetCapture(hwnd);
            }
            return 0;

        case WM_LBUTTONUP:
            if (window != nullptr) {
                window->OnMouseUp(window->ToClientDip(
                    GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
                ReleaseCapture();
            }
            return 0;

        case WM_KEYDOWN: {
            if (window != nullptr) {
                const unsigned int key = static_cast<unsigned int>(wparam);
                if (key == VK_TAB) {
                    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    window->MoveFocus(!shift);
                    return 0;
                }
                if (window->OnKeyDown(key)) {
                    return 0;
                }
            }
            break;
        }

        case WM_CHAR:
            if (window != nullptr &&
                window->OnChar(static_cast<wchar_t>(wparam))) {
                return 0;
            }
            break;

        case WM_GETOBJECT: {
            LRESULT result = 0;
            if (window != nullptr &&
                HandleAutomationGetObject(window, hwnd, wparam, lparam, &result)) {
                return result;
            }
            break;
        }

        case WM_IME_STARTCOMPOSITION:
            if (window != nullptr) {
                window->OnImeStart();
                return 0;
            }
            break;

        case WM_IME_COMPOSITION:
            if (window != nullptr) {
                if ((lparam & GCS_RESULTSTR) != 0) {
                    window->OnImeResult(ReadImeString(hwnd, GCS_RESULTSTR));
                }
                if ((lparam & GCS_COMPSTR) != 0) {
                    window->OnImeComposition(ReadImeString(hwnd, GCS_COMPSTR));
                }
                return 0;
            }
            break;

        case WM_IME_ENDCOMPOSITION:
            if (window != nullptr) {
                window->OnImeEnd();
                return 0;
            }
            break;

        case WM_KEYUP:
            if (window != nullptr &&
                window->OnKeyUp(static_cast<unsigned int>(wparam))) {
                return 0;
            }
            break;

        case WM_KILLFOCUS:
            if (window != nullptr) {
                // Keep the logical focus so typing resumes when the window is
                // activated again; only transient layers go away.
                window->ClosePopup();
                window->CancelPointerPress();
            }
            break;

        case WM_SETFOCUS:
            if (window != nullptr) {
                window->RenderFrame();
            }
            break;

        case WM_TIMER:
            if (window != nullptr && wparam == kCommitTimerId) {
                window->Present();
                return 0;
            }
            if (window != nullptr && wparam == kLiveResizeTimerId) {
                window->RenderLiveResize();
                return 0;
            }
            if (window != nullptr && wparam == kResizeSettleTimerId) {
                window->CompactResize();
                return 0;
            }
            if (window != nullptr && wparam == kToolTipTimerId) {
                window->OnToolTipTimer();
                return 0;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC device_context = BeginPaint(hwnd, &paint);
            if (window == nullptr || window->graphics_ == nullptr) {
                FillRect(
                    device_context,
                    &paint.rcPaint,
                    static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            }
            EndPaint(hwnd, &paint);
            if (window != nullptr) {
                window->RenderPendingResize();
            }
            return 0;
        }

        case WM_DPICHANGED: {
            const auto* suggested = reinterpret_cast<const RECT*>(lparam);
            SetWindowPos(
                hwnd,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

Result<int> RunMessageLoop() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (message.message == WM_QUIT) {
        return Result<int>::Ok(static_cast<int>(message.wParam));
    }

    return Result<int>::Fail(
        Error{ErrorCode::PlatformFailure, "GetMessageW failed with error " +
                                               std::to_string(GetLastError())});
}

}  // namespace nopeui
