#include "nopeui/layout/element.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>

#include "nopeui/theme/theme.h"

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
    if (!enabled_ || !Contains(point)) {
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

const Theme& Element::Colors() const noexcept {
    const Element* node = this;
    while (node->theme_ == nullptr && node->parent_ != nullptr) {
        node = node->parent_;
    }
    if (node->theme_ != nullptr) {
        return *node->theme_;
    }
    return LightTheme();
}

void Element::Invalidate() noexcept {
    Element* root = this;
    while (root->parent_ != nullptr) {
        root = root->parent_;
    }
    if (root->invalidate_) {
        root->invalidate_();
    }
}

void Element::InvalidateLayout() noexcept {
    Element* root = this;
    while (root->parent_ != nullptr) {
        root = root->parent_;
    }
    if (root->relayout_) {
        root->relayout_();
    } else if (root->invalidate_) {
        root->invalidate_();
    }
}

void Element::CollectFocusable(std::vector<Element*>& elements) noexcept {
    if (!enabled_) {
        return;
    }
    if (IsFocusable()) {
        elements.push_back(this);
    }
}

void Element::SetEnabled(bool enabled) noexcept {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    Invalidate();
}

bool Element::BubbleWheel(int delta) noexcept {
    Element* node = this;
    while (node != nullptr) {
        if (node->OnWheel(delta)) {
            return true;
        }
        node = node->parent_;
    }
    return false;
}

void Element::PushContentClip(ID2D1RenderTarget* target) noexcept {
    if (target == nullptr) {
        return;
    }
    target->PushAxisAlignedClip(
        D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
        D2D1_ANTIALIAS_MODE_ALIASED);
}

void Element::PopContentClip(ID2D1RenderTarget* target) noexcept {
    if (target != nullptr) {
        target->PopAxisAlignedClip();
    }
}

}  // namespace nopeui
