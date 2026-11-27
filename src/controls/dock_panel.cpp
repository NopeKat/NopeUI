#include "nopeui/controls/dock_panel.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>

#include <algorithm>
#include <iterator>

#include "nopeui/base/geometry.h"
#include "nopeui/layout/attached.h"

namespace nopeui {

namespace {

const PropertyMetadata kDockPanelProperties[] = {
    {L"LastChildFill", PropertyValueType::Bool, PropertyFlags::AffectsArrange,
     PropertyValue(true)},
};

DockSide ReadDock(const Element& element) noexcept {
    const PropertyValue* stored = element.FindAttachedProperty(DockProperty());
    int value = 0;
    if (stored != nullptr) {
        stored->AsInt(&value);
    }
    switch (value) {
        case 1:
            return DockSide::Top;
        case 2:
            return DockSide::Right;
        case 3:
            return DockSide::Bottom;
        default:
            return DockSide::Left;
    }
}

// Leaves the strip the child claimed and hands the rest back.
Rect Strip(Rect remaining, DockSide side, float thickness, Rect* child_slot) noexcept {
    switch (side) {
        case DockSide::Left:
            *child_slot = Rect{remaining.x, remaining.y, thickness, remaining.height};
            remaining.x += thickness;
            remaining.width = std::max(0.0f, remaining.width - thickness);
            break;
        case DockSide::Right:
            *child_slot = Rect{
                remaining.Right() - thickness, remaining.y, thickness, remaining.height};
            remaining.width = std::max(0.0f, remaining.width - thickness);
            break;
        case DockSide::Top:
            *child_slot = Rect{remaining.x, remaining.y, remaining.width, thickness};
            remaining.y += thickness;
            remaining.height = std::max(0.0f, remaining.height - thickness);
            break;
        case DockSide::Bottom:
            *child_slot = Rect{
                remaining.x, remaining.Bottom() - thickness, remaining.width, thickness};
            remaining.height = std::max(0.0f, remaining.height - thickness);
            break;
    }
    return remaining;
}

}  // namespace

Ref<DockPanel> DockPanel::Create() noexcept {
    return Ref<DockPanel>(new DockPanel());
}

void DockPanel::Add(Ref<Element> child) noexcept {
    if (!child) {
        return;
    }
    child->SetParent(this);
    children_.push_back(std::move(child));
    InvalidateLayout();
}

void DockPanel::SetLastChildFill(bool fill) noexcept {
    if (last_child_fill_ == fill) {
        return;
    }
    last_child_fill_ = fill;
    InvalidateLayout();
}

const PropertyMetadata* DockPanel::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kDockPanelProperties);
    }
    return kDockPanelProperties;
}

bool DockPanel::GetControlProperty(
    std::wstring_view name,
    PropertyValue* value) const noexcept {
    if (name == L"LastChildFill") {
        *value = PropertyValue(last_child_fill_);
        return true;
    }
    return false;
}

bool DockPanel::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    if (name != L"LastChildFill") {
        return false;
    }
    bool fill = true;
    if (!value.AsBool(&fill)) {
        return false;
    }
    SetLastChildFill(fill);
    return true;
}

Size DockPanel::MeasureOverride(Size available) noexcept {
    Size remaining{available.width, available.height};
    float width = 0.0f;
    float height = 0.0f;
    for (std::size_t index = 0; index < children_.size(); ++index) {
        const auto& child = children_[index];
        const bool filling =
            last_child_fill_ && index + 1 == children_.size();
        child->Measure(remaining);
        const Size desired = child->DesiredSize();
        if (filling) {
            width += desired.width;
            height += desired.height;
            break;
        }
        const DockSide side = ReadDock(*child);
        if (side == DockSide::Left || side == DockSide::Right) {
            width += desired.width;
            height = std::max(height, desired.height);
            remaining.width = std::max(0.0f, remaining.width - desired.width);
        } else {
            height += desired.height;
            width = std::max(width, desired.width);
            remaining.height = std::max(0.0f, remaining.height - desired.height);
        }
    }
    return Size{
        std::min(width, available.width),
        std::min(height, available.height)};
}

Size DockPanel::ArrangeOverride(Size final_size) noexcept {
    Rect remaining{bounds_.x, bounds_.y, final_size.width, final_size.height};
    for (std::size_t index = 0; index < children_.size(); ++index) {
        const auto& child = children_[index];
        const bool filling =
            last_child_fill_ && index + 1 == children_.size();
        if (filling) {
            child->Arrange(remaining);
            break;
        }
        const Size desired = child->DesiredSize();
        Rect slot{};
        const DockSide side = ReadDock(*child);
        if (side == DockSide::Left || side == DockSide::Right) {
            remaining = Strip(remaining, side, desired.width, &slot);
        } else {
            remaining = Strip(remaining, side, desired.height, &slot);
        }
        child->Arrange(slot);
    }
    return final_size;
}

void DockPanel::PaintContent(ID2D1RenderTarget* target) noexcept {
    PushContentClip(target);
    for (const auto& child : children_) {
        child->Paint(target);
    }
    PopContentClip(target);
}

Element* DockPanel::HitTestOverride(Point point) noexcept {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Element* hit = (*it)->HitTest(point);
        if (hit != nullptr) {
            return hit;
        }
    }
    return nullptr;
}

void DockPanel::CollectFocusable(std::vector<Element*>& elements) noexcept {
    if (!IsEnabled()) {
        return;
    }
    for (const auto& child : children_) {
        child->CollectFocusable(elements);
    }
}

void DockPanel::CollectChildren(std::vector<Element*>& elements) noexcept {
    for (const auto& child : children_) {
        elements.push_back(child.Get());
    }
}

}  // namespace nopeui
