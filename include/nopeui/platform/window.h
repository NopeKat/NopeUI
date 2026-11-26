#ifndef NOPEUI_PLATFORM_WINDOW_H_
#define NOPEUI_PLATFORM_WINDOW_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <string_view>

#include "nopeui/base/ref.h"
#include "nopeui/base/result.h"

namespace nopeui {

class WindowGraphics;

class Window : public RefCounted {
public:
    static Result<Ref<Window>> Create(int width, int height, std::u16string_view title);

    ~Window() override;

    void Show() noexcept;
    void Hide() noexcept;
    void Close() noexcept;
    bool IsVisible() const noexcept;

    double LastFrameMilliseconds() const noexcept;
    std::uint64_t FrameCount() const noexcept;
    void RenderFrame() noexcept;

    HWND Handle() const noexcept;

private:
    Window() noexcept;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static bool EnsureWindowClass() noexcept;

    void OnSize(int width, int height) noexcept;
    void ReleaseGraphics() noexcept;

    HWND hwnd_ = nullptr;
    std::unique_ptr<WindowGraphics> graphics_;
};

Result<int> RunMessageLoop();

}  // namespace nopeui

#endif  // NOPEUI_PLATFORM_WINDOW_H_
