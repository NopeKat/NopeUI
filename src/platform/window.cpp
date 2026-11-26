#include "nopeui/platform/window.h"

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

}  // namespace

namespace nopeui {

bool Window::EnsureWindowClass() noexcept {
    static std::once_flag once;
    static bool registered = false;

    std::call_once(once, [] {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = &Window::WndProc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
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
    return Result<Ref<Window>>::Ok(std::move(window));
}

Window::~Window() {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void Window::Show() noexcept {
    if (hwnd_ != nullptr) {
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        UpdateWindow(hwnd_);
    }
}

void Window::Hide() noexcept {
    if (hwnd_ != nullptr) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void Window::Close() noexcept {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
}

bool Window::IsVisible() const noexcept {
    return hwnd_ != nullptr && IsWindowVisible(hwnd_) != FALSE;
}

HWND Window::Handle() const noexcept {
    return hwnd_;
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
                window->hwnd_ = nullptr;
            }
            PostQuitMessage(0);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            BeginPaint(hwnd, &paint);
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
