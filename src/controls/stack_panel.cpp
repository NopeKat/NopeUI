#include "nopeui/controls/stack_panel.h"

#include <algorithm>
#include <cmath>

#include "nopeui/base/geometry.h"

namespace nopeui {

Ref<StackPanel> StackPanel::Create(float spacing) noexcept {
    Ref<StackPanel> panel(new StackPanel());
    panel->spacing_ = std::max(0.0f, spacing);
    return panel;
}

void StackPanel::Add(Ref<Element> child) noexcept {
    if (!child) {
        return;
    }
    child->SetParent(this);
    children_.push_back(std::move(child));
}

void StackPanel::SetOrientation(Orientation orientation) noexcept {
    if (orientation_ == orientation) {
        return;
    }
    orientation_ = orientation;
    Invalidate();
}

Size StackPanel::MeasureOverride(Size available) noexcept {
    float width = 0.0f;
    float height = 0.0f;
    const bool vertical = orientation_ == Orientation::Vertical;
    for (const auto& child : children_) {
        child->Measure(available);
        const Size measured = child->DesiredSize();
        if (vertical) {
            width = std::max(width, measured.width);
            height += measured.height;
        } else {
            width += measured.width;
            height = std::max(height, measured.height);
        }
    }
    if (!children_.empty()) {
        if (vertical) {
            height += spacing_ * static_cast<float>(children_.size() - 1);
        } else {
            width += spacing_ * static_cast<float>(children_.size() - 1);
        }
    }
    return Size{
        std::min(width, available.width),
        std::min(height, available.height)};
}

Size StackPanel::ArrangeOverride(Size final_size) noexcept {
    const bool vertical = orientation_ == Orientation::Vertical;
    float offset = vertical ? bounds_.y : bounds_.x;
    for (const auto& child : children_) {
        const Size measured = child->DesiredSize();
        if (vertical) {
            const float width = std::min(measured.width, final_size.width);
            child->Arrange(Rect{bounds_.x, offset, width, measured.height});
            offset += measured.height + spacing_;
        } else {
            const float width = std::min(measured.width, final_size.width);
            const float height = std::min(measured.height, final_size.height);
            child->Arrange(Rect{offset, bounds_.y, width, height});
            offset += measured.width + spacing_;
        }
    }
    return final_size;
}

void StackPanel::PaintContent(ID2D1RenderTarget* target) noexcept {
    PushContentClip(target);
    for (const auto& child : children_) {
        child->Paint(target);
    }
    PopContentClip(target);
}

Element* StackPanel::HitTestOverride(Point point) noexcept {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Element* hit = (*it)->HitTest(point);
        if (hit != nullptr) {
            return hit;
        }
    }
    return nullptr;
}

void StackPanel::CollectFocusable(std::vector<Element*>& elements) noexcept {
    for (const auto& child : children_) {
        child->CollectFocusable(elements);
    }
}

}  // namespace nopeui
