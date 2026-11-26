#include "nopeui/controls/slider.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>
#include <wrl/client.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kTrackHeight = 4.0f;
constexpr float kThumbRadius = 7.0f;
constexpr float kPreferredWidth = 280.0f;
constexpr float kPreferredHeight = 24.0f;

const PropertyMetadata kSliderProperties[] = {
    {L"Value", PropertyValueType::Double, PropertyFlags::AffectsRender, PropertyValue(0.0)},
    {L"Minimum", PropertyValueType::Double, PropertyFlags::AffectsMeasure, PropertyValue(0.0)},
    {L"Maximum", PropertyValueType::Double, PropertyFlags::AffectsMeasure, PropertyValue(1.0)},
};

}  // namespace

Ref<Slider> Slider::Create(
    double value,
    double minimum,
    double maximum) noexcept {
    Ref<Slider> control(new Slider());
    control->minimum_ = minimum;
    control->maximum_ = std::max(minimum, maximum);
    control->value_ = std::clamp(value, control->minimum_, control->maximum_);
    return control;
}

void Slider::SetValue(double value) noexcept {
    const double clamped = std::clamp(value, minimum_, maximum_);
    if (clamped == value_) {
        return;
    }
    value_ = clamped;
    Invalidate();
    if (value_changed_) {
        value_changed_(value_);
    }
}

void Slider::SetRange(double minimum, double maximum) noexcept {
    minimum_ = minimum;
    maximum_ = std::max(minimum, maximum);
    value_ = std::clamp(value_, minimum_, maximum_);
    Invalidate();
}

Size Slider::MeasureOverride(Size available) noexcept {
    return Size{
        std::min(kPreferredWidth, available.width),
        std::min(kPreferredHeight, available.height)};
}

float Slider::TrackLeft() const noexcept {
    return bounds_.x + kThumbRadius;
}

float Slider::TrackRight() const noexcept {
    return std::max(TrackLeft(), bounds_.Right() - kThumbRadius);
}

float Slider::ThumbCenter() const noexcept {
    const float left = TrackLeft();
    const float right = TrackRight();
    const double range = maximum_ - minimum_;
    const float progress = range > 0.0
        ? static_cast<float>((value_ - minimum_) / range)
        : 0.0f;
    return left + (right - left) * std::clamp(progress, 0.0f, 1.0f);
}

void Slider::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    const float center_y = bounds_.y + bounds_.height / 2.0f;
    const float left = TrackLeft();
    const float right = TrackRight();
    const float thumb = ThumbCenter();
    const float radius = kTrackHeight / 2.0f;

    ComPtr<ID2D1SolidColorBrush> track_brush;
    if (FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.scrollbar_track),
            track_brush.GetAddressOf()))) {
        return;
    }
    target->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(left, center_y - radius, right, center_y + radius),
            radius,
            radius),
        track_brush.Get());

    const std::uint32_t fill = IsEnabled() ? colors.accent : colors.button_disabled;
    ComPtr<ID2D1SolidColorBrush> fill_brush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(fill), fill_brush.GetAddressOf()))) {
        target->FillRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(left, center_y - radius, thumb, center_y + radius),
                radius,
                radius),
            fill_brush.Get());
    }

    ComPtr<ID2D1SolidColorBrush> thumb_brush;
    const std::uint32_t thumb_fill = IsEnabled() ? 0xFFFFFF : 0xE6E6E6;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(thumb_fill), thumb_brush.GetAddressOf()))) {
        target->FillEllipse(
            D2D1::Ellipse(
                D2D1::Point2F(thumb, center_y),
                kThumbRadius,
                kThumbRadius),
            thumb_brush.Get());
    }

    ComPtr<ID2D1SolidColorBrush> outline;
    const std::uint32_t border = IsEnabled()
        ? colors.button_border
        : colors.button_disabled_border;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(border), outline.GetAddressOf()))) {
        target->DrawEllipse(
            D2D1::Ellipse(
                D2D1::Point2F(thumb, center_y),
                kThumbRadius,
                kThumbRadius),
            outline.Get(),
            1.0f);
    }

    if (IsFocused() && IsEnabled()) {
        ComPtr<ID2D1SolidColorBrush> focus;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(colors.accent), focus.GetAddressOf()))) {
            target->DrawRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(
                        bounds_.x + 1.0f,
                        bounds_.y + 1.0f,
                        bounds_.Right() - 1.0f,
                        bounds_.Bottom() - 1.0f),
                    3.0f,
                    3.0f),
                focus.Get(),
                1.0f);
        }
    }
}

void Slider::OnPointerEnter() noexcept {
    hovered_ = true;
    Invalidate();
}

void Slider::OnPointerLeave() noexcept {
    hovered_ = false;
    if (!dragging_) {
        Invalidate();
    }
}

void Slider::OnPointerDown(Point point) noexcept {
    if (!IsEnabled() || !Contains(point)) {
        return;
    }
    dragging_ = true;
    hovered_ = true;
    SetValueFromPoint(point);
}

void Slider::OnPointerMove(Point point) noexcept {
    if (dragging_) {
        SetValueFromPoint(point);
    }
}

void Slider::OnPointerUp(Point point) noexcept {
    if (!dragging_) {
        return;
    }
    SetValueFromPoint(point);
    dragging_ = false;
    Invalidate();
}

bool Slider::IsFocusable() const noexcept {
    return IsEnabled();
}

bool Slider::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled()) {
        return false;
    }
    const double step = Step();
    switch (key) {
        case VK_LEFT:
        case VK_DOWN:
            SetValue(value_ - step);
            return true;
        case VK_RIGHT:
        case VK_UP:
            SetValue(value_ + step);
            return true;
        case VK_HOME:
            SetValue(minimum_);
            return true;
        case VK_END:
            SetValue(maximum_);
            return true;
        case VK_PRIOR:
            SetValue(value_ + step * 5.0);
            return true;
        case VK_NEXT:
            SetValue(value_ - step * 5.0);
            return true;
        default:
            return false;
    }
}

double Slider::Step() const noexcept {
    const double range = maximum_ - minimum_;
    return range > 0.0 ? range / 20.0 : 1.0;
}

void Slider::SetValueFromPoint(Point point) noexcept {
    const float left = TrackLeft();
    const float right = TrackRight();
    if (right <= left) {
        return;
    }
    const float progress = std::clamp(
        (point.x - left) / (right - left),
        0.0f,
        1.0f);
    SetValue(minimum_ + (maximum_ - minimum_) * progress);
}

const PropertyMetadata* Slider::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kSliderProperties);
    }
    return kSliderProperties;
}

bool Slider::GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept {
    if (name == L"Value") {
        *value = PropertyValue(value_);
        return true;
    }
    if (name == L"Minimum") {
        *value = PropertyValue(minimum_);
        return true;
    }
    if (name == L"Maximum") {
        *value = PropertyValue(maximum_);
        return true;
    }
    return false;
}

bool Slider::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    double number = 0.0;
    if (!value.AsDouble(&number)) {
        return false;
    }
    if (name == L"Value") {
        SetValue(number);
        return true;
    }
    if (name == L"Minimum") {
        SetRange(number, maximum_);
        return true;
    }
    if (name == L"Maximum") {
        SetRange(minimum_, number);
        return true;
    }
    return false;
}

AccessibleRole Slider::AccessibilityRole() const noexcept {
    return AccessibleRole::Slider;
}

bool Slider::HasAccessibilityRange() const noexcept {
    return true;
}

double Slider::AccessibilityRangeValue() const noexcept {
    return value_;
}

double Slider::AccessibilityRangeMinimum() const noexcept {
    return minimum_;
}

double Slider::AccessibilityRangeMaximum() const noexcept {
    return maximum_;
}

double Slider::AccessibilityRangeSmallChange() const noexcept {
    return Step();
}

double Slider::AccessibilityRangeLargeChange() const noexcept {
    return Step() * 5.0;
}

bool Slider::IsAccessibilityRangeReadOnly() const noexcept {
    return false;
}

bool Slider::SetAccessibilityRangeValue(double value) {
    if (!IsEnabled()) {
        return false;
    }
    SetValue(value);
    return true;
}

}  // namespace nopeui
