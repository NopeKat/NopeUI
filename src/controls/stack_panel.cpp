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

Size StackPanel::MeasureOverride(Size available) noexcept {
    float width = 0.0f;
    float height = 0.0f;
    for (const auto& child : children_) {
        child->Measure(available);
        const Size measured = child->DesiredSize();
        width = std::max(width, measured.width);
        height += measured.height;
    }
    if (!children_.empty()) {
        height += spacing_ * static_cast<float>(children_.size() - 1);
    }
    return Size{
        std::min(width, available.width),
        std::min(height, available.height)};
}

Size StackPanel::ArrangeOverride(Size final_size) noexcept {
    float y = bounds_.y;
    for (const auto& child : children_) {
        const Size measured = child->DesiredSize();
        const float width = std::min(measured.width, final_size.width);
        child->Arrange(Rect{bounds_.x, y, width, measured.height});
        y += measured.height + spacing_;
    }
    return final_size;
}

void StackPanel::PaintContent(ID2D1RenderTarget* target) noexcept {
    for (const auto& child : children_) {
        child->Paint(target);
    }
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
