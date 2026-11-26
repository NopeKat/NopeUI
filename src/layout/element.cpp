#include "nopeui/layout/element.h"

namespace nopeui {

void Element::Measure(Size available) noexcept {
    desired_ = MeasureOverride(available);
}

void Element::Arrange(const Rect& bounds) noexcept {
    bounds_ = bounds;
    ArrangeOverride(Size{bounds.width, bounds.height});
}

Size Element::ArrangeOverride(Size final_size) noexcept {
    return final_size;
}

bool Element::Contains(Point point) const noexcept {
    return point.x >= bounds_.x && point.x < bounds_.Right() &&
           point.y >= bounds_.y && point.y < bounds_.Bottom();
}

Element* Element::HitTest(Point point) noexcept {
    if (!Contains(point)) {
        return nullptr;
    }
    Element* child = HitTestOverride(point);
    if (child != nullptr) {
        return child;
    }
    return this;
}

Element* Element::HitTestOverride(Point) noexcept {
    return nullptr;
}

void Element::Paint(ID2D1RenderTarget* target) noexcept {
    if (target != nullptr) {
        PaintContent(target);
    }
}

}  // namespace nopeui
