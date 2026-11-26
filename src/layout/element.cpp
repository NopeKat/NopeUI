#include "nopeui/layout/element.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>

#include <string_view>

#include "nopeui/theme/theme.h"

namespace nopeui {

namespace {

const PropertyMetadata kBaseProperties[] = {
    {L"IsEnabled", PropertyValueType::Bool, PropertyFlags::AffectsRender, PropertyValue(true)},
    {L"Foreground", PropertyValueType::Color,
     PropertyFlags::Inherits | PropertyFlags::AffectsRender,
     PropertyValue(static_cast<std::uint32_t>(0))},
};

const PropertyMetadata* FindMetadata(
    const PropertyMetadata* table,
    std::size_t count,
    std::wstring_view name) noexcept {
    for (std::size_t index = 0; index < count; ++index) {
        if (std::wstring_view(table[index].name) == name) {
            return &table[index];
        }
    }
    return nullptr;
}

bool TypeMatches(PropertyValueType type, const PropertyValue& value) noexcept {
    return type == value.Type();
}

}  // namespace

Element::~Element() {
    if (context_menu_ != nullptr) {
        context_menu_->Release();
        context_menu_ = nullptr;
    }
}

void Element::SetForeground(std::uint32_t color) noexcept {
    if (foreground_ == color) {
        return;
    }
    foreground_ = color;
    Invalidate();
}

std::uint32_t Element::Foreground() const noexcept {
    const Element* node = this;
    while (node != nullptr) {
        if (node->foreground_ != 0) {
            return node->foreground_;
        }
        node = node->parent_;
    }
    return 0;
}

bool Element::GetProperty(std::wstring_view name, PropertyValue* value) const noexcept {
    if (name == L"IsEnabled") {
        *value = PropertyValue(enabled_);
        return true;
    }
    if (name == L"Foreground") {
        *value = PropertyValue(Foreground());
        return true;
    }

    std::size_t count = 0;
    const PropertyMetadata* table = ControlPropertyTable(&count);
    const PropertyMetadata* metadata = FindMetadata(table, count, name);
    if (metadata == nullptr) {
        return false;
    }
    if (GetControlProperty(name, value)) {
        return true;
    }
    *value = metadata->default_value;
    return true;
}

bool Element::SetProperty(std::wstring_view name, const PropertyValue& value) noexcept {
    if (name == L"IsEnabled") {
        bool enabled = true;
        if (!value.AsBool(&enabled)) {
            return false;
        }
        const bool old = enabled_;
        SetEnabled(enabled);
        if (old != enabled_) {
            property_changed_.Raise(
                kBaseProperties[0], PropertyValue(old), PropertyValue(enabled_));
        }
        return true;
    }
    if (name == L"Foreground") {
        std::uint32_t color = 0;
        if (!value.AsColor(&color)) {
            return false;
        }
        const std::uint32_t old = foreground_;
        SetForeground(color);
        if (old != foreground_) {
            property_changed_.Raise(
                kBaseProperties[1], PropertyValue(old), PropertyValue(foreground_));
        }
        return true;
    }

    std::size_t count = 0;
    const PropertyMetadata* table = ControlPropertyTable(&count);
    const PropertyMetadata* metadata = FindMetadata(table, count, name);
    if (metadata == nullptr || !TypeMatches(metadata->type, value)) {
        return false;
    }
    PropertyValue old = metadata->default_value;
    GetControlProperty(name, &old);
    if (!SetControlProperty(name, value)) {
        return false;
    }
    property_changed_.Raise(*metadata, old, value);
    if (HasFlag(metadata->flags, PropertyFlags::AffectsMeasure)) {
        InvalidateLayout();
    } else if (HasFlag(metadata->flags, PropertyFlags::AffectsRender)) {
        Invalidate();
    }
    return true;
}

std::vector<const PropertyMetadata*> Element::Properties() const {
    std::vector<const PropertyMetadata*> result;
    for (const auto& metadata : kBaseProperties) {
        result.push_back(&metadata);
    }
    std::size_t count = 0;
    const PropertyMetadata* table = ControlPropertyTable(&count);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(&table[index]);
    }
    return result;
}

const PropertyMetadata* Element::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = 0;
    }
    return nullptr;
}

bool Element::GetControlProperty(std::wstring_view, PropertyValue*) const noexcept {
    return false;
}

bool Element::SetControlProperty(std::wstring_view, const PropertyValue&) noexcept {
    return false;
}

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
