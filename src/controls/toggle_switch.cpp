#include "nopeui/controls/toggle_switch.h"

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
#include <utility>

#include "nopeui/controls/control_state.h"
#include "nopeui/graphics/text.h"
#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kTrackWidth = 40.0f;
constexpr float kTrackHeight = 20.0f;
constexpr float kKnobRadius = 7.0f;
constexpr float kKnobMargin = 3.0f;
constexpr float kGap = 8.0f;

}  // namespace

Ref<ToggleSwitch> ToggleSwitch::Create(std::wstring label) noexcept {
    Ref<ToggleSwitch> control(new ToggleSwitch());
    control->label_ = std::move(label);
    auto run = graphics::TextRun::Create(control->label_, graphics::TextStyle{});
    if (run.ok()) {
        control->run_ = std::move(run.value());
    }
    return control;
}

void ToggleSwitch::SetChecked(bool checked) noexcept {
    if (checked_ == checked) {
        return;
    }
    checked_ = checked;
    Invalidate();
    if (checked_changed_) {
        checked_changed_(checked_);
    }
}

Size ToggleSwitch::MeasureOverride(Size available) noexcept {
    const Size text = run_ != nullptr
        ? run_->Measure()
        : Size{static_cast<float>(label_.size()) * 8.0f, 20.0f};
    return Size{
        std::min(kTrackWidth + kGap + text.width, available.width),
        std::min(std::max(kTrackHeight, text.height), available.height)};
}

void ToggleSwitch::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    const ControlState state = ResolveControlState(IsEnabled(), hovered_, pressed_);
    const float track_y = bounds_.y + (bounds_.height - kTrackHeight) / 2.0f;

    std::uint32_t fill = colors.button;
    if (checked_) {
        fill = state == ControlState::Disabled ? colors.button_disabled : colors.accent;
    } else {
        switch (state) {
            case ControlState::Disabled:
                fill = colors.button_disabled;
                break;
            case ControlState::Pressed:
                fill = colors.button_pressed;
                break;
            case ControlState::Hovered:
                fill = colors.button_hover;
                break;
            case ControlState::Normal:
                break;
        }
    }
    const std::uint32_t border = state == ControlState::Disabled
        ? colors.button_disabled_border
        : (checked_ ? fill : colors.button_border);

    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(D2D1::ColorF(fill), brush.GetAddressOf()))) {
        return;
    }
    target->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(
                bounds_.x,
                track_y,
                bounds_.x + kTrackWidth,
                track_y + kTrackHeight),
            kTrackHeight / 2.0f,
            kTrackHeight / 2.0f),
        brush.Get());

    ComPtr<ID2D1SolidColorBrush> outline;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(border), outline.GetAddressOf()))) {
        target->DrawRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(
                    bounds_.x + 0.5f,
                    track_y + 0.5f,
                    bounds_.x + kTrackWidth - 0.5f,
                    track_y + kTrackHeight - 0.5f),
                kTrackHeight / 2.0f,
                kTrackHeight / 2.0f),
            outline.Get(),
            1.0f);
    }

    const float knob_x = checked_
        ? bounds_.x + kTrackWidth - kKnobMargin - kKnobRadius
        : bounds_.x + kKnobMargin + kKnobRadius;
    const std::uint32_t knob_color = state == ControlState::Disabled
        ? 0xE6E6E6
        : 0xFFFFFF;
    ComPtr<ID2D1SolidColorBrush> knob;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(knob_color), knob.GetAddressOf()))) {
        target->FillEllipse(
            D2D1::Ellipse(
                D2D1::Point2F(knob_x, track_y + kTrackHeight / 2.0f),
                kKnobRadius,
                kKnobRadius),
            knob.Get());
    }

    if (run_ != nullptr) {
        const Size text = run_->Measure();
        const std::uint32_t color = state == ControlState::Disabled
            ? colors.text_disabled
            : colors.text;
        run_->Draw(
            target,
            Point{
                bounds_.x + kTrackWidth + kGap,
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
                        bounds_.y + 2.0f,
                        bounds_.x + kTrackWidth - 2.0f,
                        bounds_.Bottom() - 2.0f),
                    4.0f,
                    4.0f),
                focus.Get(),
                1.0f);
        }
    }
}

void ToggleSwitch::OnPointerEnter() noexcept {
    hovered_ = true;
    Invalidate();
}

void ToggleSwitch::OnPointerLeave() noexcept {
    hovered_ = false;
    pressed_ = false;
    Invalidate();
}

void ToggleSwitch::OnPointerDown(Point point) noexcept {
    if (Contains(point)) {
        pressed_ = true;
        Invalidate();
    }
}

void ToggleSwitch::OnPointerUp(Point point) noexcept {
    const bool activate = pressed_ && Contains(point);
    pressed_ = false;
    Invalidate();
    if (activate) {
        Toggle();
    }
}

bool ToggleSwitch::IsFocusable() const noexcept {
    return IsEnabled();
}

bool ToggleSwitch::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled() || key != VK_SPACE) {
        return false;
    }
    pressed_ = true;
    Invalidate();
    return true;
}

bool ToggleSwitch::OnKeyUp(unsigned int key) noexcept {
    if (!IsEnabled() || key != VK_SPACE) {
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

void ToggleSwitch::Toggle() noexcept {
    checked_ = !checked_;
    Invalidate();
    if (checked_changed_) {
        checked_changed_(checked_);
    }
}

AccessibleRole ToggleSwitch::AccessibilityRole() const noexcept {
    return AccessibleRole::Button;
}

std::wstring ToggleSwitch::AccessibilityName() const {
    return label_;
}

bool ToggleSwitch::HasAccessibilityToggle() const noexcept {
    return true;
}

bool ToggleSwitch::AccessibilityToggleOn() const noexcept {
    return checked_;
}

void ToggleSwitch::ToggleAccessibility() noexcept {
    if (IsEnabled()) {
        Toggle();
    }
}

}  // namespace nopeui
