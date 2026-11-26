#include "nopeui/platform/window.h"

#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>

namespace {

constexpr const wchar_t* kWindowClassName = L"NopeUI.Window";

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
        surface_width_ = width;
        surface_height_ = height;
        return Result<void>::Ok();
    }

    void Render() noexcept {
        if (virtual_surface_ == nullptr || d2d_context_ == nullptr) {
            return;
        }

        const auto start = std::chrono::steady_clock::now();

        ComPtr<IDXGISurface> dxgi_surface;
        POINT offset{};
        HRESULT result = virtual_surface_->BeginDraw(
            nullptr,
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

        d2d_context_->SetTarget(target_bitmap_.Get());
        d2d_context_->SetTransform(D2D1::Matrix3x2F::Translation(
            static_cast<float>(offset.x),
            static_cast<float>(offset.y)));
        d2d_context_->BeginDraw();
        d2d_context_->Clear(D2D1::ColorF(0xF3F3F3));
        d2d_context_->FillRectangle(
            D2D1::RectF(0.0f, 0.0f, static_cast<float>(surface_width_), 4.0f),
            accent_brush_.Get());
        result = d2d_context_->EndDraw();
        d2d_context_->SetTarget(nullptr);

        virtual_surface_->EndDraw();
        if (FAILED(result)) {
            return;
        }

        result = dcomp_device_->Commit();
        if (FAILED(result)) {
            return;
        }

        const auto elapsed = std::chrono::steady_clock::now() - start;
        last_frame_ms_ =
            std::chrono::duration<double, std::milli>(elapsed).count();
        ++frame_count_;
    }

    double LastFrameMilliseconds() const noexcept {
        return last_frame_ms_;
    }

    std::uint64_t FrameCount() const noexcept {
        return frame_count_;
    }

private:
    WindowGraphics() = default;

    Result<void> Initialize(HWND hwnd, int width, int height) {
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

        Render();
        return Result<void>::Ok();
    }

    void ReleaseSurface() noexcept {
        target_bitmap_.Reset();
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
    ComPtr<ID2D1SolidColorBrush> accent_brush_;
    int surface_width_ = 0;
    int surface_height_ = 0;
    double last_frame_ms_ = 0.0;
    std::uint64_t frame_count_ = 0;
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
            graphics_->Render();
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

void Window::RenderFrame() noexcept {
    if (graphics_ != nullptr) {
        graphics_->Render();
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
        graphics_->Render();
    }
}

void Window::ReleaseGraphics() noexcept {
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
