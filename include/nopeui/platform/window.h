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
#include "nopeui/theme/theme.h"

namespace nopeui {

class OverlayRoot;
class StyleSheet;
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
    // width and height are logical DIPs; the native window is scaled to the
    // monitor DPI, so 900x600 stays the same apparent size at 125% or 150%.
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

    // Anchored popup layer above the content. The point is the preferred
    // top-left in client space; the layer clamps it into the client area.
    void ShowPopup(Ref<Element> popup, Point anchor) noexcept;
    void ClosePopup() noexcept;
    bool HasPopup() const noexcept;

    // Applies selectors to the current content tree and keeps the sheet for
    // future content. Later styles override earlier ones.
    void SetStyleSheet(Ref<StyleSheet> style_sheet) noexcept;
    StyleSheet* GetStyleSheet() const noexcept;

    // The window owns the theme and exposes it to the content tree. Changing
    // it repaints the current content.
    void SetTheme(Theme theme) noexcept;
    const Theme& GetTheme() const noexcept;

    HWND Handle() const noexcept;
    // DPI scale for client coordinates: 1.0 at 96 DPI.
    float DpiScale() const noexcept {
        return scale_;
    }

    // Accessibility integration: the content root and current keyboard focus.
    Element* ContentElement() const noexcept {
        return content_.Get();
    }
    Element* FocusedElement() const noexcept {
        return focused_;
    }
    void FocusFromAccessibility(Element* element) noexcept;

private:
    Window() noexcept;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static bool EnsureWindowClass() noexcept;

    void OnSize(int width, int height) noexcept;
    void ApplyDpi(UINT dpi) noexcept;
    void OnSizing(const RECT& proposed) noexcept;
    void BeginLiveResize() noexcept;
    void EndLiveResize() noexcept;
    void RenderLiveResize() noexcept;
    void RenderPendingResize() noexcept;
    void ApplyResize(int width, int height) noexcept;
    void ScheduleResizeSettle() noexcept;
    void CompactResize() noexcept;
    void ReleaseGraphics() noexcept;
    void RecreateGraphics() noexcept;

    void LayoutContent(int width, int height) noexcept;
    void RenderGraphics(const RECT* dirty_rect, bool force_commit) noexcept;
    void UpdateClientMetrics(int pixel_width, int pixel_height) noexcept;
    Point ToClientDip(int pixel_x, int pixel_y) const noexcept;
    void OnMouseMove(Point point) noexcept;
    void OnMouseDown(Point point) noexcept;
    void OnMouseUp(Point point) noexcept;
    void OnRightMouseDown(Point point) noexcept;
    void OnMouseLeave() noexcept;
    void CancelPointerPress() noexcept;
    void OnMouseWheel(Point point, int delta) noexcept;
    void OnToolTipTimer() noexcept;
    void UpdateToolTipTimer() noexcept;
    std::wstring ToolTipFor(const Element* element) const;
    void FocusElement(Element* element) noexcept;
    void MoveFocus(bool forward) noexcept;
    void ClearFocus() noexcept;
    bool OnKeyDown(unsigned int key) noexcept;
    bool OnKeyUp(unsigned int key) noexcept;
    bool OnChar(wchar_t character) noexcept;
    void OnImeStart() noexcept;
    void OnImeComposition(std::wstring_view text) noexcept;
    void OnImeResult(std::wstring_view text) noexcept;
    void OnImeEnd() noexcept;
    void ApplyStyleSheet() noexcept;

    HWND hwnd_ = nullptr;
    std::unique_ptr<WindowGraphics> graphics_;
    Theme theme_;
    OverlayRoot* root_ = nullptr;
    Ref<Element> content_;
    StyleSheet* style_sheet_ = nullptr;
    Element* hovered_ = nullptr;
    Element* pressed_ = nullptr;
    Element* focused_ = nullptr;
    Element* previous_focus_ = nullptr;
    Point pointer_{};
    bool tracking_mouse_ = false;
    bool popup_close_pending_ = false;
    std::uint64_t pending_close_generation_ = 0;
    bool popup_is_tooltip_ = false;
    bool tooltip_timer_active_ = false;
    int client_width_ = 0;
    int client_height_ = 0;
    int client_pixel_width_ = 0;
    int client_pixel_height_ = 0;
    float scale_ = 1.0f;
    int pending_width_ = 0;
    int pending_height_ = 0;
    bool resize_pending_ = false;
    // Interactive drags stretch the last frame and rebuild the swap chain
    // buffers once the drag settles, instead of on every mouse move.
    bool resize_deferred_ = false;
    bool live_resizing_ = false;
    bool recreate_pending_ = false;
    bool dpi_change_active_ = false;
    UINT pending_dpi_ = 0;
    double last_resize_ms_ = 0.0;
};

Result<int> RunMessageLoop();

}  // namespace nopeui

#endif  // NOPEUI_PLATFORM_WINDOW_H_
