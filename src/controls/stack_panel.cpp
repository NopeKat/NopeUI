#include "nopeui/controls/stack_panel.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "nopeui/base/geometry.h"

namespace nopeui {

namespace {

const PropertyMetadata kStackPanelProperties[] = {
    {L"Spacing", PropertyValueType::Float, PropertyFlags::AffectsMeasure, PropertyValue(12.0f)},
    {L"Orientation", PropertyValueType::Int, PropertyFlags::AffectsMeasure, PropertyValue(0)},
};

}  // namespace
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

void StackPanel::SetSpacing(float spacing) noexcept {
    const float value = std::max(0.0f, spacing);
    if (spacing_ == value) {
        return;
    }
    spacing_ = value;
    InvalidateLayout();
}

const PropertyMetadata* StackPanel::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kStackPanelProperties);
    }
    return kStackPanelProperties;
}

bool StackPanel::GetControlProperty(
    std::wstring_view name,
    PropertyValue* value) const noexcept {
    if (name == L"Spacing") {
        *value = PropertyValue(spacing_);
        return true;
    }
    if (name == L"Orientation") {
        *value = PropertyValue(orientation_ == Orientation::Vertical ? 0 : 1);
        return true;
    }
    return false;
}

bool StackPanel::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    if (name == L"Spacing") {
        float spacing = 0.0f;
        if (!value.AsFloat(&spacing)) {
            return false;
        }
        SetSpacing(spacing);
        return true;
    }
    if (name == L"Orientation") {
        int orientation = 0;
        if (!value.AsInt(&orientation)) {
            return false;
        }
        if (orientation == 0) {
            SetOrientation(Orientation::Vertical);
        } else if (orientation == 1) {
            SetOrientation(Orientation::Horizontal);
        } else {
            return false;
        }
        return true;
    }
    return false;
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
        width,
        std::min(height, available.height)};
}

Size StackPanel::ArrangeOverride(Size final_size) noexcept {
    const bool vertical = orientation_ == Orientation::Vertical;
    float offset = vertical ? bounds_.y : bounds_.x;
    for (const auto& child : children_) {
        const Size measured = child->DesiredSize();
        // The cross axis gets the full extent: the child's own alignment decides
        // whether it stretches, stays at its measured size or centers.
        if (vertical) {
            child->Arrange(Rect{bounds_.x, offset, final_size.width, measured.height});
            offset += measured.height + spacing_;
        } else {
            child->Arrange(Rect{offset, bounds_.y, measured.width, final_size.height});
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
    if (!IsEnabled()) {
        return;
    }
    for (const auto& child : children_) {
        child->CollectFocusable(elements);
    }
}

void StackPanel::CollectChildren(std::vector<Element*>& elements) noexcept {
    for (const auto& child : children_) {
        elements.push_back(child.Get());
    }
}

}  // namespace nopeui
