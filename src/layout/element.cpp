#include "nopeui/layout/element.h"

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
#include <string_view>

#include "nopeui/layout/attached.h"
#include "nopeui/theme/theme.h"

namespace nopeui {

namespace {

const PropertyMetadata kIsEnabledProperty{
    L"IsEnabled", PropertyValueType::Bool, PropertyFlags::AffectsRender, PropertyValue(true)};
const PropertyMetadata kForegroundProperty{
    L"Foreground", PropertyValueType::Color,
    PropertyFlags::Inherits | PropertyFlags::AffectsRender,
    PropertyValue(static_cast<std::uint32_t>(0))};

// Layout inputs live on every element, so a panel never has to know which
// child types accept margins. Alignment uses the enum order as its int value.
const PropertyMetadata kLayoutProperties[] = {
    {L"Margin", PropertyValueType::Thickness, PropertyFlags::AffectsMeasure,
     PropertyValue(Thickness{})},
    {L"Width", PropertyValueType::Float, PropertyFlags::AffectsMeasure, PropertyValue(0.0f)},
    {L"Height", PropertyValueType::Float, PropertyFlags::AffectsMeasure, PropertyValue(0.0f)},
    {L"MinWidth", PropertyValueType::Float, PropertyFlags::AffectsMeasure, PropertyValue(0.0f)},
    {L"MinHeight", PropertyValueType::Float, PropertyFlags::AffectsMeasure, PropertyValue(0.0f)},
    {L"MaxWidth", PropertyValueType::Float, PropertyFlags::AffectsMeasure, PropertyValue(0.0f)},
    {L"MaxHeight", PropertyValueType::Float, PropertyFlags::AffectsMeasure, PropertyValue(0.0f)},
    {L"HorizontalAlignment", PropertyValueType::Int, PropertyFlags::AffectsMeasure,
     PropertyValue(static_cast<int>(HorizontalAlignment::Stretch))},
    {L"VerticalAlignment", PropertyValueType::Int, PropertyFlags::AffectsMeasure,
     PropertyValue(static_cast<int>(VerticalAlignment::Stretch))},
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

const PropertyMetadata* FindBaseMetadata(std::wstring_view name) noexcept {
    if (name == L"IsEnabled") {
        return &kIsEnabledProperty;
    }
    if (name == L"Foreground") {
        return &kForegroundProperty;
    }
    return FindMetadata(kLayoutProperties, std::size(kLayoutProperties), name);
}

bool TypeMatches(PropertyValueType type, const PropertyValue& value) noexcept {
    return type == value.Type();
}

float ClampWidth(
    float value,
    float explicit_width,
    float min_width,
    float max_width) noexcept {
    float width = explicit_width > 0.0f ? explicit_width : value;
    if (max_width > 0.0f) {
        width = std::min(width, max_width);
    }
    if (min_width > 0.0f) {
        width = std::max(width, min_width);
    }
    return std::max(0.0f, width);
}

float ClampHeight(
    float value,
    float explicit_height,
    float min_height,
    float max_height) noexcept {
    float height = explicit_height > 0.0f ? explicit_height : value;
    if (max_height > 0.0f) {
        height = std::min(height, max_height);
    }
    if (min_height > 0.0f) {
        height = std::max(height, min_height);
    }
    return std::max(0.0f, height);
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

void Element::SetMargin(Thickness margin) noexcept {
    if (margin_ == margin) {
        return;
    }
    margin_ = margin;
    InvalidateLayout();
}

void Element::SetWidth(float width) noexcept {
    const float value = std::max(0.0f, width);
    if (width_ == value) {
        return;
    }
    width_ = value;
    InvalidateLayout();
}

void Element::SetHeight(float height) noexcept {
    const float value = std::max(0.0f, height);
    if (height_ == value) {
        return;
    }
    height_ = value;
    InvalidateLayout();
}

void Element::SetMinWidth(float width) noexcept {
    const float value = std::max(0.0f, width);
    if (min_width_ == value) {
        return;
    }
    min_width_ = value;
    InvalidateLayout();
}

void Element::SetMinHeight(float height) noexcept {
    const float value = std::max(0.0f, height);
    if (min_height_ == value) {
        return;
    }
    min_height_ = value;
    InvalidateLayout();
}

void Element::SetMaxWidth(float width) noexcept {
    const float value = std::max(0.0f, width);
    if (max_width_ == value) {
        return;
    }
    max_width_ = value;
    InvalidateLayout();
}

void Element::SetMaxHeight(float height) noexcept {
    const float value = std::max(0.0f, height);
    if (max_height_ == value) {
        return;
    }
    max_height_ = value;
    InvalidateLayout();
}

void Element::SetHorizontalAlignment(HorizontalAlignment alignment) noexcept {
    if (horizontal_alignment_ == alignment) {
        return;
    }
    horizontal_alignment_ = alignment;
    InvalidateLayout();
}

void Element::SetVerticalAlignment(VerticalAlignment alignment) noexcept {
    if (vertical_alignment_ == alignment) {
        return;
    }
    vertical_alignment_ = alignment;
    InvalidateLayout();
}

bool Element::SetAttachedProperty(
    const AttachedPropertyMetadata& metadata,
    const PropertyValue& value) noexcept {
    if (!TypeMatches(metadata.type, value)) {
        return false;
    }
    for (auto& entry : attached_) {
        if (entry.first == &metadata) {
            entry.second = value;
            return true;
        }
    }
    attached_.emplace_back(&metadata, value);
    return true;
}

bool Element::SetAttachedProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    const AttachedPropertyMetadata* metadata = nopeui::FindAttachedProperty(name);
    if (metadata == nullptr) {
        return false;
    }
    return SetAttachedProperty(*metadata, value);
}

bool Element::GetAttachedProperty(std::wstring_view name, PropertyValue* value) const noexcept {
    const AttachedPropertyMetadata* metadata = nopeui::FindAttachedProperty(name);
    if (metadata == nullptr) {
        return false;
    }
    if (const PropertyValue* stored = FindAttachedProperty(*metadata)) {
        *value = *stored;
    } else {
        *value = metadata->default_value;
    }
    return true;
}

const PropertyValue* Element::FindAttachedProperty(
    const AttachedPropertyMetadata& metadata) const noexcept {
    for (const auto& entry : attached_) {
        if (entry.first == &metadata) {
            return &entry.second;
        }
    }
    return nullptr;
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
    if (name == L"Margin") {
        *value = PropertyValue(margin_);
        return true;
    }
    if (name == L"Width") {
        *value = PropertyValue(width_);
        return true;
    }
    if (name == L"Height") {
        *value = PropertyValue(height_);
        return true;
    }
    if (name == L"MinWidth") {
        *value = PropertyValue(min_width_);
        return true;
    }
    if (name == L"MinHeight") {
        *value = PropertyValue(min_height_);
        return true;
    }
    if (name == L"MaxWidth") {
        *value = PropertyValue(max_width_);
        return true;
    }
    if (name == L"MaxHeight") {
        *value = PropertyValue(max_height_);
        return true;
    }
    if (name == L"HorizontalAlignment") {
        *value = PropertyValue(static_cast<int>(horizontal_alignment_));
        return true;
    }
    if (name == L"VerticalAlignment") {
        *value = PropertyValue(static_cast<int>(vertical_alignment_));
        return true;
    }
    if (IsAttachedPropertyName(name)) {
        return GetAttachedProperty(name, value);
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
    return SetPropertyCore(name, value, true);
}

bool Element::ApplyStyleProperty(std::wstring_view name, const PropertyValue& value) noexcept {
    if (HasLocalProperty(name)) {
        return true;
    }
    return SetPropertyCore(name, value, false);
}

bool Element::HasLocalProperty(std::wstring_view name) const noexcept {
    for (const std::wstring& local : local_properties_) {
        if (std::wstring_view(local) == name) {
            return true;
        }
    }
    return false;
}

void Element::RecordLocalProperty(std::wstring_view name) noexcept {
    if (HasLocalProperty(name)) {
        return;
    }
    local_properties_.emplace_back(name);
}

bool Element::SetPropertyCore(
    std::wstring_view name,
    const PropertyValue& value,
    bool local) noexcept {
    if (name == L"IsEnabled") {
        bool enabled = true;
        if (!value.AsBool(&enabled)) {
            return false;
        }
        const bool old = enabled_;
        SetEnabled(enabled);
        if (local) {
            RecordLocalProperty(name);
        }
        if (old != enabled_) {
            property_changed_.Raise(
                kIsEnabledProperty, PropertyValue(old), PropertyValue(enabled_));
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
        if (local) {
            RecordLocalProperty(name);
        }
        if (old != foreground_) {
            property_changed_.Raise(
                kForegroundProperty, PropertyValue(old), PropertyValue(foreground_));
        }
        return true;
    }

    if (const PropertyMetadata* metadata = FindBaseMetadata(name);
        metadata != nullptr && metadata != &kIsEnabledProperty &&
        metadata != &kForegroundProperty) {
        if (!TypeMatches(metadata->type, value)) {
            return false;
        }
        PropertyValue previous = metadata->default_value;
        GetProperty(name, &previous);
        if (name == L"Margin") {
            Thickness margin;
            if (!value.AsThickness(&margin)) {
                return false;
            }
            SetMargin(margin);
        } else if (name == L"Width" || name == L"Height" || name == L"MinWidth" ||
                   name == L"MinHeight" || name == L"MaxWidth" || name == L"MaxHeight") {
            float number = 0.0f;
            if (!value.AsFloat(&number)) {
                return false;
            }
            if (name == L"Width") {
                SetWidth(number);
            } else if (name == L"Height") {
                SetHeight(number);
            } else if (name == L"MinWidth") {
                SetMinWidth(number);
            } else if (name == L"MinHeight") {
                SetMinHeight(number);
            } else if (name == L"MaxWidth") {
                SetMaxWidth(number);
            } else {
                SetMaxHeight(number);
            }
        } else {
            int alignment = 0;
            if (!value.AsInt(&alignment) || alignment < 0 || alignment > 3) {
                return false;
            }
            if (name == L"HorizontalAlignment") {
                SetHorizontalAlignment(static_cast<HorizontalAlignment>(alignment));
            } else {
                SetVerticalAlignment(static_cast<VerticalAlignment>(alignment));
            }
        }
        if (local) {
            RecordLocalProperty(name);
        }
        property_changed_.Raise(*metadata, previous, value);
        return true;
    }

    if (IsAttachedPropertyName(name)) {
        if (!TypeMatches(nopeui::FindAttachedProperty(name)->type, value)) {
            return false;
        }
        const bool applied = SetAttachedProperty(name, value);
        if (applied && local) {
            RecordLocalProperty(name);
        }
        return applied;
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
    if (local) {
        RecordLocalProperty(name);
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
    result.push_back(&kIsEnabledProperty);
    result.push_back(&kForegroundProperty);
    for (const auto& metadata : kLayoutProperties) {
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

Size Element::ApplyMeasureConstraints(Size available) noexcept {
    Size content{
        std::max(0.0f, available.width - margin_.Horizontal()),
        std::max(0.0f, available.height - margin_.Vertical())};
    if (width_ > 0.0f) {
        content.width = std::min(content.width, width_);
    }
    if (height_ > 0.0f) {
        content.height = std::min(content.height, height_);
    }
    if (max_width_ > 0.0f) {
        content.width = std::min(content.width, max_width_);
    }
    if (max_height_ > 0.0f) {
        content.height = std::min(content.height, max_height_);
    }
    if (min_width_ > 0.0f) {
        content.width = std::max(content.width, min_width_);
    }
    if (min_height_ > 0.0f) {
        content.height = std::max(content.height, min_height_);
    }
    return content;
}

Size Element::ApplySizeConstraints(Size content) const noexcept {
    return Size{
        ClampWidth(content.width, width_, min_width_, max_width_),
        ClampHeight(content.height, height_, min_height_, max_height_)};
}

Rect Element::ResolveSlot(const Rect& slot) const noexcept {
    Rect content{
        slot.x + margin_.left,
        slot.y + margin_.top,
        std::max(0.0f, slot.width - margin_.Horizontal()),
        std::max(0.0f, slot.height - margin_.Vertical())};

    float width = content_desired_.width;
    float height = content_desired_.height;
    if (width_ > 0.0f) {
        width = width_;
    }
    if (height_ > 0.0f) {
        height = height_;
    }
    width = ClampWidth(width, width_, min_width_, max_width_);
    height = ClampHeight(height, height_, min_height_, max_height_);

    float x = content.x;
    switch (horizontal_alignment_) {
        case HorizontalAlignment::Left:
            break;
        case HorizontalAlignment::Center:
            x = content.x + (content.width - width) / 2.0f;
            break;
        case HorizontalAlignment::Right:
            x = content.Right() - width;
            break;
        case HorizontalAlignment::Stretch:
            width = ClampWidth(content.width, width_, min_width_, max_width_);
            x = content.x + (content.width - width) / 2.0f;
            break;
    }

    float y = content.y;
    switch (vertical_alignment_) {
        case VerticalAlignment::Top:
            break;
        case VerticalAlignment::Center:
            y = content.y + (content.height - height) / 2.0f;
            break;
        case VerticalAlignment::Bottom:
            y = content.Bottom() - height;
            break;
        case VerticalAlignment::Stretch:
            height = ClampHeight(content.height, height_, min_height_, max_height_);
            y = content.y + (content.height - height) / 2.0f;
            break;
    }

    return Rect{x, y, width, height};
}

void Element::Measure(Size available) noexcept {
    content_desired_ = MeasureOverride(ApplyMeasureConstraints(available));
    const Size content = ApplySizeConstraints(content_desired_);
    content_desired_ = content;
    desired_ = Size{
        content.width + margin_.Horizontal(),
        content.height + margin_.Vertical()};
}

void Element::Arrange(const Rect& slot) noexcept {
    bounds_ = ResolveSlot(slot);
    ArrangeOverride(Size{bounds_.width, bounds_.height});
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
