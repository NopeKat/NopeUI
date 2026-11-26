#include "nopeui/controls/toggle_button.h"

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

#include "nopeui/controls/control_state.h"
#include "nopeui/graphics/text.h"
#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

const PropertyMetadata kToggleButtonProperties[] = {
    {L"Text", PropertyValueType::String, PropertyFlags::None, PropertyValue(std::wstring())},
    {L"IsChecked", PropertyValueType::Bool, PropertyFlags::AffectsRender, PropertyValue(false)},
};

constexpr float kPaddingX = 16.0f;
constexpr float kPaddingY = 8.0f;
constexpr float kMinWidth = 88.0f;
constexpr float kMinHeight = 32.0f;
constexpr float kCornerRadius = 4.0f;

}  // namespace

Ref<ToggleButton> ToggleButton::Create(std::wstring text) noexcept {
    Ref<ToggleButton> button(new ToggleButton());
    button->text_ = std::move(text);
    auto run = graphics::TextRun::Create(button->text_, graphics::TextStyle{});
    if (run.ok()) {
        button->run_ = std::move(run.value());
    }
    return button;
}

void ToggleButton::SetChecked(bool checked) noexcept {
    if (checked_ == checked) {
        return;
    }
    checked_ = checked;
    Invalidate();
    if (checked_changed_) {
        checked_changed_(checked_);
    }
}

Size ToggleButton::MeasureOverride(Size available) noexcept {
    const Size text = run_ != nullptr
        ? run_->Measure()
        : Size{static_cast<float>(text_.size()) * 8.0f, 20.0f};
    return Size{
        std::min(std::max(text.width + kPaddingX * 2.0f, kMinWidth), available.width),
        std::min(std::max(text.height + kPaddingY * 2.0f, kMinHeight), available.height)};
}

void ToggleButton::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    const ControlState state = ResolveControlState(IsEnabled(), hovered_, pressed_);
    std::uint32_t background = colors.button;
    if (checked_) {
        background = state == ControlState::Disabled ? colors.button_disabled : colors.accent;
    } else {
        switch (state) {
            case ControlState::Disabled:
                background = colors.button_disabled;
                break;
            case ControlState::Pressed:
                background = colors.button_pressed;
                break;
            case ControlState::Hovered:
                background = colors.button_hover;
                break;
            case ControlState::Normal:
                break;
        }
    }
    const std::uint32_t border = state == ControlState::Disabled
        ? colors.button_disabled_border
        : (checked_ ? background : colors.button_border);
    const std::uint32_t text_color = state == ControlState::Disabled
        ? colors.text_disabled
        : (checked_ ? colors.background : colors.text);

    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(background), brush.GetAddressOf()))) {
        return;
    }
    target->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
            kCornerRadius,
            kCornerRadius),
        brush.Get());

    ComPtr<ID2D1SolidColorBrush> outline;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(border), outline.GetAddressOf()))) {
        target->DrawRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(
                    bounds_.x + 0.5f,
                    bounds_.y + 0.5f,
                    bounds_.Right() - 0.5f,
                    bounds_.Bottom() - 0.5f),
                kCornerRadius,
                kCornerRadius),
            outline.Get(),
            1.0f);
    }

    if (run_ != nullptr) {
        const Size text = run_->Measure();
        run_->Draw(
            target,
            Point{
                bounds_.x + (bounds_.width - text.width) / 2.0f,
                bounds_.y + (bounds_.height - text.height) / 2.0f},
            text_color);
    }

    if (IsFocused() && IsEnabled()) {
        ComPtr<ID2D1SolidColorBrush> focus;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(checked_ ? colors.text : colors.accent), focus.GetAddressOf()))) {
            target->DrawRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(
                        bounds_.x + 2.5f,
                        bounds_.y + 2.5f,
                        bounds_.Right() - 2.5f,
                        bounds_.Bottom() - 2.5f),
                    kCornerRadius,
                    kCornerRadius),
                focus.Get(),
                1.0f);
        }
    }
}

void ToggleButton::OnPointerEnter() noexcept {
    hovered_ = true;
    Invalidate();
}

void ToggleButton::OnPointerLeave() noexcept {
    hovered_ = false;
    pressed_ = false;
    Invalidate();
}

void ToggleButton::OnPointerDown(Point point) noexcept {
    if (Contains(point)) {
        pressed_ = true;
        hovered_ = true;
        Invalidate();
    }
}

void ToggleButton::OnPointerUp(Point point) noexcept {
    const bool activate = pressed_ && Contains(point);
    pressed_ = false;
    Invalidate();
    if (activate) {
        Toggle();
    }
}

bool ToggleButton::IsFocusable() const noexcept {
    return IsEnabled();
}

bool ToggleButton::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled() || (key != VK_SPACE && key != VK_RETURN)) {
        return false;
    }
    if (!pressed_) {
        pressed_ = true;
        Invalidate();
    }
    return true;
}

bool ToggleButton::OnKeyUp(unsigned int key) noexcept {
    if (!IsEnabled() || (key != VK_SPACE && key != VK_RETURN)) {
        return false;
    }
    const bool activate = pressed_;
    pressed_ = false;
    Invalidate();
    if (activate) {
        Toggle();
    }
    return true;
}

void ToggleButton::Toggle() noexcept {
    checked_ = !checked_;
    Invalidate();
    if (checked_changed_) {
        checked_changed_(checked_);
    }
}

const PropertyMetadata* ToggleButton::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kToggleButtonProperties);
    }
    return kToggleButtonProperties;
}

bool ToggleButton::GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept {
    if (name == L"Text") {
        *value = PropertyValue(text_);
        return true;
    }
    if (name == L"IsChecked") {
        *value = PropertyValue(checked_);
        return true;
    }
    return false;
}

bool ToggleButton::SetControlProperty(
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

AccessibleRole ToggleButton::AccessibilityRole() const noexcept {
    return AccessibleRole::Button;
}

std::wstring ToggleButton::AccessibilityName() const {
    return text_;
}

bool ToggleButton::HasAccessibilityInvoke() const noexcept {
    return true;
}

bool ToggleButton::HasAccessibilityToggle() const noexcept {
    return true;
}

bool ToggleButton::AccessibilityToggleOn() const noexcept {
    return checked_;
}

void ToggleButton::InvokeAccessibility() noexcept {
    if (IsEnabled()) {
        Toggle();
    }
}

void ToggleButton::ToggleAccessibility() noexcept {
    if (IsEnabled()) {
        Toggle();
    }
}

}  // namespace nopeui
