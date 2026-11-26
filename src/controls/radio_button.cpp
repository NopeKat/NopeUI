#include "nopeui/controls/radio_button.h"

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
#include <vector>

#include "nopeui/controls/control_state.h"
#include "nopeui/graphics/text.h"
#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kCircleSize = 18.0f;
constexpr float kGap = 8.0f;
constexpr float kDotSize = 8.0f;

const PropertyMetadata kRadioButtonProperties[] = {
    {L"Label", PropertyValueType::String, PropertyFlags::None, PropertyValue(std::wstring())},
    {L"IsChecked", PropertyValueType::Bool, PropertyFlags::AffectsRender, PropertyValue(false)},
};

}  // namespace

Ref<RadioButton> RadioButton::Create(std::wstring label) noexcept {
    Ref<RadioButton> button(new RadioButton());
    button->label_ = std::move(label);
    auto run = graphics::TextRun::Create(button->label_, graphics::TextStyle{});
    if (run.ok()) {
        button->run_ = std::move(run.value());
    }
    return button;
}

void RadioButton::SetChecked(bool checked) noexcept {
    if (checked_ == checked) {
        return;
    }
    if (checked && Parent() != nullptr) {
        std::vector<Element*> siblings;
        Parent()->CollectChildren(siblings);
        for (Element* sibling : siblings) {
            if (sibling != this) {
                sibling->DeselectRadioGroup(this);
            }
        }
    }
    checked_ = checked;
    Invalidate();
    if (checked_changed_) {
        checked_changed_(checked_);
    }
}

Size RadioButton::MeasureOverride(Size available) noexcept {
    const Size text = run_ != nullptr
        ? run_->Measure()
        : Size{static_cast<float>(label_.size()) * 8.0f, 20.0f};
    return Size{
        std::min(kCircleSize + kGap + text.width, available.width),
        std::min(std::max(kCircleSize, text.height), available.height)};
}

void RadioButton::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    const ControlState state = ResolveControlState(IsEnabled(), hovered_, pressed_);
    const float circle_y = bounds_.y + (bounds_.height - kCircleSize) / 2.0f;
    const D2D1_POINT_2F center{
        bounds_.x + kCircleSize / 2.0f,
        circle_y + kCircleSize / 2.0f};

    std::uint32_t fill = colors.button;
    switch (state) {
        case ControlState::Disabled:
            fill = colors.button_disabled;
            break;
        case ControlState::Pressed:
            fill = checked_ ? colors.accent : colors.button_pressed;
            break;
        case ControlState::Hovered:
            fill = checked_ ? colors.accent : colors.button_hover;
            break;
        case ControlState::Normal:
            fill = checked_ ? colors.accent : colors.button;
            break;
    }
    const std::uint32_t border = state == ControlState::Disabled
        ? colors.button_disabled_border
        : (checked_ ? fill : colors.button_border);

    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(D2D1::ColorF(fill), brush.GetAddressOf()))) {
        return;
    }
    target->FillEllipse(
        D2D1::Ellipse(center, kCircleSize / 2.0f, kCircleSize / 2.0f),
        brush.Get());

    ComPtr<ID2D1SolidColorBrush> outline;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(border), outline.GetAddressOf()))) {
        target->DrawEllipse(
            D2D1::Ellipse(center, kCircleSize / 2.0f, kCircleSize / 2.0f),
            outline.Get(),
            1.0f);
    }

    if (checked_) {
        const std::uint32_t dot_color = state == ControlState::Disabled
            ? colors.text_disabled
            : colors.background;
        ComPtr<ID2D1SolidColorBrush> dot;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(dot_color), dot.GetAddressOf()))) {
            target->FillEllipse(
                D2D1::Ellipse(center, kDotSize / 2.0f, kDotSize / 2.0f),
                dot.Get());
        }
    }

    if (run_ != nullptr) {
        const Size text = run_->Measure();
        const std::uint32_t color = state == ControlState::Disabled
            ? colors.text_disabled
            : colors.text;
        run_->Draw(
            target,
            Point{
                bounds_.x + kCircleSize + kGap,
                bounds_.y + (bounds_.height - text.height) / 2.0f},
            color);
    }

    if (IsFocused() && IsEnabled()) {
        ComPtr<ID2D1SolidColorBrush> focus;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(colors.accent), focus.GetAddressOf()))) {
            target->DrawRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(
                        bounds_.x + 2.0f,
                        bounds_.y + 1.5f,
                        bounds_.Right() - 2.0f,
                        bounds_.Bottom() - 1.5f),
                    2.0f,
                    2.0f),
                focus.Get(),
                1.0f);
        }
    }
}

void RadioButton::OnPointerEnter() noexcept {
    hovered_ = true;
    Invalidate();
}

void RadioButton::OnPointerLeave() noexcept {
    hovered_ = false;
    pressed_ = false;
    Invalidate();
}

void RadioButton::OnPointerDown(Point point) noexcept {
    if (Contains(point)) {
        pressed_ = true;
        Invalidate();
    }
}

void RadioButton::OnPointerUp(Point point) noexcept {
    const bool activate = pressed_ && Contains(point);
    pressed_ = false;
    Invalidate();
    if (activate) {
        Select();
    }
}

bool RadioButton::IsFocusable() const noexcept {
    return IsEnabled();
}

bool RadioButton::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled() || key != VK_SPACE) {
        return false;
    }
    pressed_ = true;
    Invalidate();
    return true;
}

bool RadioButton::OnKeyUp(unsigned int key) noexcept {
    if (!IsEnabled() || key != VK_SPACE) {
        return false;
    }
    const bool activate = pressed_;
    pressed_ = false;
    Invalidate();
    if (activate) {
        Select();
    }
    return true;
}

void RadioButton::Select() noexcept {
    SetChecked(true);
}

void RadioButton::DeselectRadioGroup(const Element* source) noexcept {
    if (source == this || !checked_) {
        return;
    }
    checked_ = false;
    Invalidate();
    if (checked_changed_) {
        checked_changed_(false);
    }
}

const PropertyMetadata* RadioButton::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kRadioButtonProperties);
    }
    return kRadioButtonProperties;
}

bool RadioButton::GetControlProperty(
    std::wstring_view name,
    PropertyValue* value) const noexcept {
    if (name == L"Label") {
        *value = PropertyValue(label_);
        return true;
    }
    if (name == L"IsChecked") {
        *value = PropertyValue(checked_);
        return true;
    }
    return false;
}

bool RadioButton::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    if (name != L"IsChecked") {
        return false;
    }
    bool checked = false;
    if (!value.AsBool(&checked)) {
        return false;
    }
    SetChecked(checked);
    return true;
}

AccessibleRole RadioButton::AccessibilityRole() const noexcept {
    return AccessibleRole::RadioButton;
}

std::wstring RadioButton::AccessibilityName() const {
    return label_;
}

bool RadioButton::HasAccessibilityToggle() const noexcept {
    return true;
}

bool RadioButton::AccessibilityToggleOn() const noexcept {
    return checked_;
}

void RadioButton::ToggleAccessibility() noexcept {
    if (IsEnabled()) {
        Select();
    }
}

}  // namespace nopeui
