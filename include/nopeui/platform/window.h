#ifndef NOPEUI_PLATFORM_WINDOW_H_
#define NOPEUI_PLATFORM_WINDOW_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <string_view>

#include "nopeui/base/ref.h"
#include "nopeui/base/result.h"

namespace nopeui {

class Window : public RefCounted {
public:
    static Result<Ref<Window>> Create(int width, int height, std::u16string_view title);

    ~Window() override;

    void Show() noexcept;
    void Hide() noexcept;
    void Close() noexcept;
    bool IsVisible() const noexcept;

    HWND Handle() const noexcept;

private:
    Window() noexcept = default;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static bool EnsureWindowClass() noexcept;

    HWND hwnd_ = nullptr;
};

Result<int> RunMessageLoop();

}  // namespace nopeui

#endif  // NOPEUI_PLATFORM_WINDOW_H_
