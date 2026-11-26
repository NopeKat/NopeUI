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
#include <span>
#include <string>
#include <string_view>

#include "nopeui/base/geometry.h"
#include "nopeui/base/ref.h"
#include "nopeui/base/result.h"
#include "nopeui/layout/element.h"

namespace nopeui {

class WindowGraphics;

struct FrameTimings {
    std::uint64_t sequence = 0;
    double begin_draw_ms = 0.0;
    double draw_ms = 0.0;
    double end_draw_ms = 0.0;
    double commit_ms = 0.0;
};

struct FrameLatency {
    std::uint64_t sequence = 0;
    double queue_ms = 0.0;
    double commit_ms = 0.0;
    double present_ms = 0.0;
};

struct CommitQueueStats {
    std::uint64_t submitted = 0;
    std::uint64_t completed = 0;
    std::uint64_t dropped = 0;
    std::uint64_t waits = 0;
    double total_wait_ms = 0.0;
    std::uint32_t max_depth = 0;
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
    double LastResizeMilliseconds() const noexcept;

    // Estimated queue, commit and next-vblank latency of the last submitted frame.
    FrameLatency LastPresentedFrame() const noexcept;
    CommitQueueStats AsyncCommitStats() const noexcept;
    bool IsAsyncCommitEnabled() const noexcept;
    std::uint64_t FrameCount() const noexcept;
    std::uint64_t CommitCount() const noexcept;
    void RenderFrame() noexcept;
    void RenderFrame(const Rect& dirty_rect) noexcept;

    // Benchmark-only synthetic load; not part of the stable API.
    void SetDiagnosticLoad(
        std::uint32_t rectangles,
        std::uint32_t text_runs,
        bool virtual_list = false) noexcept;
    void SetDiagnosticScroll(float offset) noexcept;
    void SetDiagnosticTextList(
        std::span<const std::wstring> rows,
        float row_height,
        bool cache_layouts) noexcept;

    // Benchmark-only override of the asynchronous surface count. Zero restores
    // the area-based selection (three up to 2560x1600, two above).
    void SetDiagnosticAsyncSurfaces(int count) noexcept;
    int DiagnosticAsyncSurfaceCount() const noexcept;

    // Benchmark-only presentation switch: true uses a flip-model swap chain,
    // false uses the DirectComposition virtual surface path.
    void SetDiagnosticFlipModel(bool enabled) noexcept;
    bool IsDiagnosticFlipModel() const noexcept;

    // Presents the latest rendered surface immediately.
    void Present() noexcept;

    // Batches commits to at most one per display refresh. Enabled by default.
    void SetCommitThrottle(bool enabled) noexcept;

    // Alternates two or three surfaces and commits them off the UI thread.
    // Enabled by default; disabling waits for pending commits and falls back
    // to synchronous.
    void SetAsyncCommit(bool enabled) noexcept;

    // Rasterizes the scene into a WIC bitmap on the CPU and blits it into the
    // composition surface. Enabled automatically in remote sessions.
    void SetSoftwareRender(bool enabled) noexcept;
    bool IsSoftwareRender() const noexcept;

    // Sets the root element laid out over the client area and routed pointer
    // input. Passing no content restores the placeholder scene.
    void SetContent(Ref<Element> content) noexcept;
    Element* Content() const noexcept;

    HWND Handle() const noexcept;

private:
    Window() noexcept;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static bool EnsureWindowClass() noexcept;

    void OnSize(int width, int height) noexcept;
    void OnSizing(const RECT& proposed) noexcept;
    void BeginLiveResize() noexcept;
    void EndLiveResize() noexcept;
    void RenderLiveResize() noexcept;
    void RenderPendingResize() noexcept;
    void ScheduleResizeSettle() noexcept;
    void CompactResize() noexcept;
    void ReleaseGraphics() noexcept;

    void LayoutContent(int width, int height) noexcept;
    void OnMouseMove(Point point) noexcept;
    void OnMouseDown(Point point) noexcept;
    void OnMouseUp(Point point) noexcept;
    void OnMouseLeave() noexcept;

    HWND hwnd_ = nullptr;
    std::unique_ptr<WindowGraphics> graphics_;
    Ref<Element> content_;
    Element* hovered_ = nullptr;
    Element* pressed_ = nullptr;
    bool tracking_mouse_ = false;
    int client_width_ = 0;
    int client_height_ = 0;
    int pending_width_ = 0;
    int pending_height_ = 0;
    bool resize_pending_ = false;
    bool live_resizing_ = false;
    double last_resize_ms_ = 0.0;
};

Result<int> RunMessageLoop();

}  // namespace nopeui

#endif  // NOPEUI_PLATFORM_WINDOW_H_
