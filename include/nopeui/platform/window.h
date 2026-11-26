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

#include "nopeui/base/geometry.h"
#include "nopeui/base/ref.h"
#include "nopeui/base/result.h"

namespace nopeui {

class WindowGraphics;

struct FrameTimings {
    double begin_draw_ms = 0.0;
    double draw_ms = 0.0;
    double end_draw_ms = 0.0;
    double commit_ms = 0.0;
};

class Window : public RefCounted {
public:
    static Result<Ref<Window>> Create(int width, int height, std::u16string_view title);

    ~Window() override;

    void Show() noexcept;
    void Hide() noexcept;
    void Close() noexcept;
    bool IsVisible() const noexcept;

    double LastFrameMilliseconds() const noexcept;
    FrameTimings LastFrameTimings() const noexcept;
    std::uint64_t FrameCount() const noexcept;
    std::uint64_t CommitCount() const noexcept;
    void RenderFrame() noexcept;
    void RenderFrame(const Rect& dirty_rect) noexcept;

    // Presents the latest rendered surface immediately.
    void Present() noexcept;

    // Batches commits to at most one per display refresh. Enabled by default.
    void SetCommitThrottle(bool enabled) noexcept;

    HWND Handle() const noexcept;

private:
    Window() noexcept;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static bool EnsureWindowClass() noexcept;

    void OnSize(int width, int height) noexcept;
    void RenderPendingResize() noexcept;
    void ReleaseGraphics() noexcept;

    HWND hwnd_ = nullptr;
    std::unique_ptr<WindowGraphics> graphics_;
    int client_width_ = 0;
    int client_height_ = 0;
    int pending_width_ = 0;
    int pending_height_ = 0;
    bool resize_pending_ = false;
};

Result<int> RunMessageLoop();

}  // namespace nopeui

#endif  // NOPEUI_PLATFORM_WINDOW_H_
