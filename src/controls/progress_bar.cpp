#include "nopeui/controls/progress_bar.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d2d1.h>
#include <wrl/client.h>

#include <algorithm>
#include <iterator>

#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kBarHeight = 6.0f;

const PropertyMetadata kProgressBarProperties[] = {
    {L"Value", PropertyValueType::Float, PropertyFlags::AffectsRender, PropertyValue(0.0f)},
    {L"IsIndeterminate", PropertyValueType::Bool, PropertyFlags::AffectsRender,
     PropertyValue(false)},
};

}  // namespace

Ref<ProgressBar> ProgressBar::Create(float value) noexcept {
    Ref<ProgressBar> control(new ProgressBar());
    control->value_ = std::clamp(value, 0.0f, 1.0f);
    return control;
}

void ProgressBar::SetValue(float value) noexcept {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    if (clamped == value_) {
        return;
    }
    value_ = clamped;
    Invalidate();
}

void ProgressBar::SetIndeterminate(bool indeterminate) noexcept {
    if (indeterminate_ == indeterminate) {
        return;
    }
    indeterminate_ = indeterminate;
    Invalidate();
}

Size ProgressBar::MeasureOverride(Size available) noexcept {
    return Size{
        std::min(available.width, 100000.0f),
        std::min(kBarHeight, available.height)};
}

void ProgressBar::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    const D2D1_ROUNDED_RECT track = D2D1::RoundedRect(
        D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
        kBarHeight / 2.0f,
        kBarHeight / 2.0f);

    ComPtr<ID2D1SolidColorBrush> track_brush;
    if (FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.scrollbar_track),
            track_brush.GetAddressOf()))) {
        return;
    }
    target->FillRoundedRectangle(track, track_brush.Get());

    float start = bounds_.x;
    float end = bounds_.x;
    if (indeterminate_) {
        start = bounds_.x + bounds_.width * 0.35f;
        end = bounds_.x + bounds_.width * 0.65f;
    } else {
        end = bounds_.x + bounds_.width * value_;
    }

    if (end <= start) {
        return;
    }
    ComPtr<ID2D1SolidColorBrush> fill_brush;
    const std::uint32_t fill = IsEnabled()
        ? colors.accent
        : colors.button_disabled;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(fill), fill_brush.GetAddressOf()))) {
        target->FillRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(start, bounds_.y, end, bounds_.Bottom()),
                kBarHeight / 2.0f,
                kBarHeight / 2.0f),
            fill_brush.Get());
    }
}

const PropertyMetadata* ProgressBar::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kProgressBarProperties);
    }
    return kProgressBarProperties;
}

bool ProgressBar::GetControlProperty(
    std::wstring_view name,
    PropertyValue* value) const noexcept {
    if (name == L"Value") {
        *value = PropertyValue(value_);
        return true;
    }
    if (name == L"IsIndeterminate") {
        *value = PropertyValue(indeterminate_);
        return true;
    }
    return false;
}

bool ProgressBar::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    if (name == L"Value") {
        float number = 0.0f;
        if (!value.AsFloat(&number)) {
            return false;
        }
        SetValue(number);
        return true;
    }
    if (name == L"IsIndeterminate") {
        bool indeterminate = false;
        if (!value.AsBool(&indeterminate)) {
            return false;
        }
        SetIndeterminate(indeterminate);
        return true;
    }
    return false;
}

AccessibleRole ProgressBar::AccessibilityRole() const noexcept {
    return AccessibleRole::ProgressBar;
}

bool ProgressBar::HasAccessibilityRange() const noexcept {
    return true;
}

double ProgressBar::AccessibilityRangeValue() const noexcept {
    return static_cast<double>(value_ * 100.0f);
}

double ProgressBar::AccessibilityRangeMinimum() const noexcept {
    return 0.0;
}

double ProgressBar::AccessibilityRangeMaximum() const noexcept {
    return 100.0;
}

double ProgressBar::AccessibilityRangeSmallChange() const noexcept {
    return 0.0;
}

double ProgressBar::AccessibilityRangeLargeChange() const noexcept {
    return 0.0;
}

bool ProgressBar::IsAccessibilityRangeReadOnly() const noexcept {
    return true;
}

}  // namespace nopeui
