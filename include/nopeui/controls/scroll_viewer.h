#ifndef NOPEUI_CONTROLS_SCROLL_VIEWER_H_
#define NOPEUI_CONTROLS_SCROLL_VIEWER_H_

#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Single child scrolled vertically by wheel input, clipped to its viewport.
// There is no scrollbar yet; the offset is clamped to the measured content.
class ScrollViewer : public Element {
public:
    static Ref<ScrollViewer> Create() noexcept;

    void SetChild(Ref<Element> child) noexcept;

    float VerticalOffset() const noexcept {
        return offset_;
    }
    void SetVerticalOffset(float offset) noexcept;

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    Element* HitTestOverride(Point point) noexcept override;
    void CollectFocusable(std::vector<Element*>& elements) noexcept override;
    bool OnWheel(int delta) noexcept override;
    void OnPointerDown(Point point) noexcept override;
    void OnPointerMove(Point point) noexcept override;
    void OnPointerUp(Point point) noexcept override;
    void BringChildIntoView(const Element* descendant) noexcept override;

private:
    ScrollViewer() noexcept = default;

    float MaxOffset() const noexcept;
    Rect TrackRect() const noexcept;
    Rect ThumbRect() const noexcept;

    Ref<Element> child_;
    float offset_ = 0.0f;
    float content_height_ = 0.0f;
    float drag_start_y_ = 0.0f;
    float drag_start_offset_ = 0.0f;
    bool dragging_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_SCROLL_VIEWER_H_
