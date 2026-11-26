#include "nopeui/platform/window.h"

#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>

namespace {

constexpr const wchar_t* kWindowClassName = L"NopeUI.Window";
constexpr UINT_PTR kCommitTimerId = 1;

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

}  // namespace

namespace nopeui {

using Microsoft::WRL::ComPtr;

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
        ReleaseSurface();
        if (visual_ != nullptr) {
            visual_->SetContent(nullptr);
        }
    }

    Result<void> Resize(int width, int height) noexcept {
        if (width <= 0 || height <= 0) {
            return Result<void>::Ok();
        }
        if (width == surface_width_ && height == surface_height_) {
            return Result<void>::Ok();
        }

        HRESULT result = S_OK;
        if (virtual_surface_ == nullptr) {
            result = dcomp_device_->CreateVirtualSurface(
                static_cast<UINT>(width),
                static_cast<UINT>(height),
                DXGI_FORMAT_B8G8R8A8_UNORM,
                DXGI_ALPHA_MODE_PREMULTIPLIED,
                virtual_surface_.GetAddressOf());
            if (FAILED(result)) {
                return Result<void>::Fail(
                    Error{ErrorCode::PlatformFailure, PlatformError("CreateVirtualSurface", result)});
            }

            result = visual_->SetContent(virtual_surface_.Get());
            if (FAILED(result)) {
                return Result<void>::Fail(
                    Error{ErrorCode::PlatformFailure, PlatformError("SetContent", result)});
            }
        } else {
            result = virtual_surface_->Resize(
                static_cast<UINT>(width), static_cast<UINT>(height));
            if (FAILED(result)) {
                return Result<void>::Fail(
                    Error{ErrorCode::PlatformFailure, PlatformError("Resize", result)});
            }
        }

        target_bitmap_.Reset();
        target_set_ = false;
        transform_set_ = false;
        surface_width_ = width;
        surface_height_ = height;
        return Result<void>::Ok();
    }

    void Render(const RECT* dirty_rect = nullptr, bool force_commit = false) noexcept {
        if (virtual_surface_ == nullptr || d2d_context_ == nullptr) {
            return;
        }

        const auto start = std::chrono::steady_clock::now();

        RECT update{};
        const RECT* update_ptr = nullptr;
        if (dirty_rect != nullptr) {
            update.left = std::max(0L, dirty_rect->left);
            update.top = std::max(0L, dirty_rect->top);
            update.right = std::min(static_cast<LONG>(surface_width_), dirty_rect->right);
            update.bottom = std::min(static_cast<LONG>(surface_height_), dirty_rect->bottom);
            if (update.right <= update.left || update.bottom <= update.top) {
                return;
            }
            update_ptr = &update;
        }

        const D2D1_RECT_F background_rect = update_ptr == nullptr
            ? D2D1::RectF(0.0f, 0.0f, static_cast<float>(surface_width_), static_cast<float>(surface_height_))
            : D2D1::RectF(
                  static_cast<float>(update.left),
                  static_cast<float>(update.top),
                  static_cast<float>(update.right),
                  static_cast<float>(update.bottom));

        ComPtr<IDXGISurface> dxgi_surface;
        POINT offset{};
        HRESULT result = virtual_surface_->BeginDraw(
            update_ptr,
            __uuidof(IDXGISurface),
            reinterpret_cast<void**>(dxgi_surface.GetAddressOf()),
            &offset);
        if (FAILED(result)) {
            return;
        }

        if (target_bitmap_ == nullptr) {
            D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            result = d2d_context_->CreateBitmapFromDxgiSurface(
                dxgi_surface.Get(), properties, target_bitmap_.GetAddressOf());
            if (FAILED(result)) {
                virtual_surface_->EndDraw();
                return;
            }
        }

        if (!target_set_) {
            d2d_context_->SetTarget(target_bitmap_.Get());
            target_set_ = true;
        }
        if (!transform_set_ || offset.x != offset_x_ || offset.y != offset_y_) {
            d2d_context_->SetTransform(D2D1::Matrix3x2F::Translation(
                static_cast<float>(offset.x),
                static_cast<float>(offset.y)));
            offset_x_ = offset.x;
            offset_y_ = offset.y;
            transform_set_ = true;
        }
        d2d_context_->BeginDraw();
        if (update_ptr == nullptr) {
            d2d_context_->Clear(D2D1::ColorF(0xF3F3F3));
        } else {
            d2d_context_->PushAxisAlignedClip(background_rect, D2D1_ANTIALIAS_MODE_ALIASED);
            d2d_context_->FillRectangle(background_rect, background_brush_.Get());
        }

        const bool draw_text = update_ptr == nullptr ||
            (update.right > 24 && update.left < 324 &&
             update.bottom > 28 && update.top < 88);
        if (draw_text) {
            if (text_bitmap_ != nullptr) {
                d2d_context_->DrawBitmap(
                    text_bitmap_.Get(),
                    D2D1::RectF(24.0f, 28.0f, 384.0f, 92.0f),
                    1.0f,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
            } else {
                d2d_context_->DrawTextLayout(
                    D2D1::Point2F(24.0f, 28.0f),
                    text_layout_.Get(),
                    text_brush_.Get());
            }
        }

        d2d_context_->FillRectangle(
            D2D1::RectF(0.0f, 0.0f, static_cast<float>(surface_width_), 4.0f),
            accent_brush_.Get());
        if (update_ptr != nullptr) {
            d2d_context_->PopAxisAlignedClip();
        }
        result = d2d_context_->EndDraw();
        if (FAILED(result)) {
            target_set_ = false;
        }

        virtual_surface_->EndDraw();
        if (FAILED(result)) {
            return;
        }

        commit_pending_ = true;
        if (force_commit || !throttle_commits_ || CommitDue()) {
            Present();
        } else {
            ScheduleCommit();
        }

        const auto elapsed = std::chrono::steady_clock::now() - start;
        last_frame_ms_ =
            std::chrono::duration<double, std::milli>(elapsed).count();
        ++frame_count_;
    }

    bool Present() noexcept {
        if (!commit_pending_ || dcomp_device_ == nullptr) {
            return false;
        }

        const HRESULT result = dcomp_device_->Commit();
        if (FAILED(result)) {
            ScheduleCommit();
            return false;
        }

        commit_pending_ = false;
        last_commit_time_ = std::chrono::steady_clock::now();
        ++commit_count_;
        CancelCommitTimer();
        return true;
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

    double LastFrameMilliseconds() const noexcept {
        return last_frame_ms_;
    }

    std::uint64_t FrameCount() const noexcept {
        return frame_count_;
    }

    std::uint64_t CommitCount() const noexcept {
        return commit_count_;
    }

private:
    WindowGraphics() = default;

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
            D2D1::ColorF(0xF3F3F3), background_brush_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateSolidColorBrush", result)});
        }

        result = d2d_context_->CreateSolidColorBrush(
            D2D1::ColorF(0x0067C0), accent_brush_.GetAddressOf());
        if (FAILED(result)) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, PlatformError("CreateSolidColorBrush", result)});
        }

        auto surface_result = Resize(width, height);
        if (!surface_result.ok()) {
            return surface_result;
        }

        Render(nullptr, true);
        return Result<void>::Ok();
    }

    void ReleaseSurface() noexcept {
        CancelCommitTimer();
        target_bitmap_.Reset();
        target_set_ = false;
        transform_set_ = false;
        virtual_surface_.Reset();
        surface_width_ = 0;
        surface_height_ = 0;
    }

    ComPtr<ID3D11Device> d3d_device_;
    ComPtr<IDXGIDevice> dxgi_device_;
    ComPtr<IDCompositionDevice> dcomp_device_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<IDCompositionVisual> visual_;
    ComPtr<IDCompositionVirtualSurface> virtual_surface_;
    ComPtr<ID2D1Factory1> d2d_factory_;
    ComPtr<ID2D1Device> d2d_device_;
    ComPtr<ID2D1DeviceContext> d2d_context_;
    ComPtr<ID2D1Bitmap1> target_bitmap_;
    ComPtr<IDWriteFactory> dwrite_factory_;
    ComPtr<IDWriteTextFormat> text_format_;
    ComPtr<IDWriteTextLayout> text_layout_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
    ComPtr<ID2D1Bitmap> text_bitmap_;
    ComPtr<ID2D1SolidColorBrush> background_brush_;
    ComPtr<ID2D1SolidColorBrush> accent_brush_;
    HWND hwnd_ = nullptr;
    int surface_width_ = 0;
    int surface_height_ = 0;
    double commit_interval_ms_ = 1000.0 / 120.0;
    double last_frame_ms_ = 0.0;
    std::chrono::steady_clock::time_point last_commit_time_{};
    std::uint64_t frame_count_ = 0;
    std::uint64_t commit_count_ = 0;
    bool throttle_commits_ = true;
    bool commit_pending_ = false;
    bool commit_timer_active_ = false;
    bool target_set_ = false;
    bool transform_set_ = false;
    LONG offset_x_ = 0;
    LONG offset_y_ = 0;
};

Window::Window() noexcept = default;

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

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        native_title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
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

    window->graphics_ = std::move(graphics).value();
    return Result<Ref<Window>>::Ok(std::move(window));
}

Window::~Window() {
    ReleaseGraphics();
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
            graphics_->Render(nullptr, true);
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

std::uint64_t Window::FrameCount() const noexcept {
    return graphics_ != nullptr ? graphics_->FrameCount() : 0;
}

std::uint64_t Window::CommitCount() const noexcept {
    return graphics_ != nullptr ? graphics_->CommitCount() : 0;
}

void Window::RenderFrame() noexcept {
    if (graphics_ != nullptr) {
        graphics_->Render();
    }
}

void Window::RenderFrame(const Rect& dirty_rect) noexcept {
    if (graphics_ == nullptr || dirty_rect.IsEmpty()) {
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

HWND Window::Handle() const noexcept {
    return hwnd_;
}

void Window::OnSize(int width, int height) noexcept {
    if (graphics_ == nullptr) {
        return;
    }

    auto resized = graphics_->Resize(width, height);
    if (resized.ok()) {
        graphics_->Render(nullptr, true);
    }
}

void Window::ReleaseGraphics() noexcept {
    if (hwnd_ != nullptr) {
        KillTimer(hwnd_, kCommitTimerId);
    }
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

        case WM_SIZE:
            if (window != nullptr && wparam != SIZE_MINIMIZED) {
                window->OnSize(LOWORD(lparam), HIWORD(lparam));
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_TIMER:
            if (window != nullptr && wparam == kCommitTimerId) {
                window->Present();
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
