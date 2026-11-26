#ifndef NOPEUI_CONTROLS_SCROLL_VIEWER_H_
#define NOPEUI_CONTROLS_SCROLL_VIEWER_H_

#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Single child scrolled by wheel input in both directions, clipped to its
// viewport. Thin scrollbars only appear on axes whose content overflows.
class ScrollViewer : public Element {
public:
    static Ref<ScrollViewer> Create() noexcept;

    void SetChild(Ref<Element> child) noexcept;

    float VerticalOffset() const noexcept {
        return offset_y_;
    }
    void SetVerticalOffset(float offset) noexcept;

    float HorizontalOffset() const noexcept {
        return offset_x_;
    }
    void SetHorizontalOffset(float offset) noexcept;

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    Element* HitTestOverride(Point point) noexcept override;
    void CollectFocusable(std::vector<Element*>& elements) noexcept override;
    void CollectChildren(std::vector<Element*>& elements) noexcept override;
    bool OnWheel(int delta) noexcept override;
    bool OnKeyDown(unsigned int key) noexcept override;
    void OnPointerEnter() noexcept override;
    void OnPointerLeave() noexcept override;
    void OnPointerDown(Point point) noexcept override;
    void OnPointerMove(Point point) noexcept override;
    void OnPointerUp(Point point) noexcept override;
    void BringChildIntoView(const Element* descendant) noexcept override;

private:
    enum class DragAxis {
        None,
        Vertical,
        Horizontal,
    };

    ScrollViewer() noexcept = default;

    float MaxVerticalOffset() const noexcept;
    float MaxHorizontalOffset() const noexcept;
    bool HasVerticalScrollbar() const noexcept;
    bool HasHorizontalScrollbar() const noexcept;
    float ScrollbarThickness() const noexcept;

    Rect VerticalTrackRect() const noexcept;
    Rect VerticalThumbRect() const noexcept;
    Rect HorizontalTrackRect() const noexcept;
    Rect HorizontalThumbRect() const noexcept;

    Ref<Element> child_;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;
    float content_width_ = 0.0f;
    float content_height_ = 0.0f;
    float drag_start_x_ = 0.0f;
    float drag_start_y_ = 0.0f;
    float drag_start_offset_x_ = 0.0f;
    float drag_start_offset_y_ = 0.0f;
    DragAxis drag_axis_ = DragAxis::None;
    bool hovered_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_SCROLL_VIEWER_H_
