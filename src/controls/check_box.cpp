#include "nopeui/controls/check_box.h"

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
#include "nopeui/theme/control_template.h"
#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kBoxSize = 18.0f;
constexpr float kGap = 8.0f;
constexpr float kCornerRadius = 3.0f;

const PropertyMetadata kCheckBoxProperties[] = {
    {L"Label", PropertyValueType::String, PropertyFlags::None, PropertyValue(std::wstring())},
    {L"IsChecked", PropertyValueType::Bool, PropertyFlags::AffectsRender, PropertyValue(false)},
};

}  // namespace

Ref<CheckBox> CheckBox::Create(std::wstring label) noexcept {
    Ref<CheckBox> box(new CheckBox());
    box->label_ = std::move(label);
    auto run = graphics::TextRun::Create(box->label_, graphics::TextStyle{});
    if (run.ok()) {
        box->run_ = std::move(run.value());
    }
    return box;
}

void CheckBox::SetChecked(bool checked) noexcept {
    if (checked_ == checked) {
        return;
    }
    const bool previous = checked_;
    checked_ = checked;
    Invalidate();
    RaiseControlPropertyChange(
        L"IsChecked", PropertyValue(previous), PropertyValue(checked_));
    if (checked_changed_) {
        checked_changed_(checked_);
    }
}

Size CheckBox::MeasureOverride(Size available) noexcept {
    const Size text = run_ != nullptr
        ? run_->Measure()
        : Size{static_cast<float>(label_.size()) * 8.0f, 20.0f};
    return Size{
        std::min(kBoxSize + kGap + text.width, available.width),
        std::min(std::max(kBoxSize, text.height), available.height)};
}

void CheckBox::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    const ControlState state = ResolveControlState(IsEnabled(), hovered_, pressed_);
    const float box_y = bounds_.y + (bounds_.height - kBoxSize) / 2.0f;

    std::uint32_t fill = colors.button;
    if (checked_) {
        fill = state == ControlState::Disabled ? colors.button_disabled : colors.accent;
        if (template_box_background_ != 0 && state != ControlState::Disabled) {
            fill = template_box_background_;
        }
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
                if (template_box_background_ != 0) {
                    fill = template_box_background_;
                }
                break;
        }
    }
    const std::uint32_t border = template_box_border_ != 0
        ? template_box_border_
        : (state == ControlState::Disabled
            ? colors.button_disabled_border
            : (checked_ ? fill : colors.button_border));

    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(D2D1::ColorF(fill), brush.GetAddressOf()))) {
        return;
    }
    target->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(bounds_.x, box_y, bounds_.x + kBoxSize, box_y + kBoxSize),
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
                    box_y + 0.5f,
                    bounds_.x + kBoxSize - 0.5f,
                    box_y + kBoxSize - 0.5f),
                kCornerRadius,
                kCornerRadius),
            outline.Get(),
            1.0f);
    }

    if (checked_) {
        const std::uint32_t check_color = state == ControlState::Disabled
            ? colors.text_disabled
            : (template_check_ != 0 ? template_check_ : colors.background);
        ComPtr<ID2D1SolidColorBrush> check;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(check_color), check.GetAddressOf()))) {
            target->DrawLine(
                D2D1::Point2F(bounds_.x + 4.0f, box_y + 9.0f),
                D2D1::Point2F(bounds_.x + 7.5f, box_y + 12.5f),
                check.Get(),
                2.0f);
            target->DrawLine(
                D2D1::Point2F(bounds_.x + 7.5f, box_y + 12.5f),
                D2D1::Point2F(bounds_.x + 14.0f, box_y + 5.0f),
                check.Get(),
                2.0f);
        }
    }

    if (run_ != nullptr) {
        const Size text = run_->Measure();
        const std::uint32_t color = state == ControlState::Disabled
            ? colors.text_disabled
            : (template_foreground_ != 0 ? template_foreground_ : ForegroundOr(colors.text));
        run_->Draw(
            target,
            Point{
                bounds_.x + kBoxSize + kGap,
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

void CheckBox::OnPointerEnter() noexcept {
    hovered_ = true;
    Invalidate();
}

void CheckBox::OnPointerLeave() noexcept {
    hovered_ = false;
    pressed_ = false;
    Invalidate();
}

void CheckBox::OnPointerDown(Point point) noexcept {
    if (Contains(point)) {
        pressed_ = true;
        Invalidate();
    }
}

void CheckBox::OnPointerUp(Point point) noexcept {
    const bool activate = pressed_ && Contains(point);
    pressed_ = false;
    Invalidate();
    if (activate) {
        Toggle();
    }
}

bool CheckBox::IsFocusable() const noexcept {
    return IsEnabled();
}

bool CheckBox::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled() || key != VK_SPACE) {
        return false;
    }
    pressed_ = true;
    Invalidate();
    return true;
}

bool CheckBox::OnKeyUp(unsigned int key) noexcept {
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

void CheckBox::Toggle() noexcept {
    SetChecked(!checked_);
}

void CheckBox::ApplyControlTemplate(const ControlTemplate& control_template) noexcept {
    if (const TemplatePart* part = control_template.Find(L"Box")) {
        template_box_background_ = part->background;
        template_box_border_ = part->border;
    }
    if (const TemplatePart* part = control_template.Find(L"Check")) {
        template_check_ = part->foreground;
    }
    if (const TemplatePart* part = control_template.Find(L"Content")) {
        template_foreground_ = part->foreground;
    }
    Invalidate();
}

const PropertyMetadata* CheckBox::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kCheckBoxProperties);
    }
    return kCheckBoxProperties;
}

bool CheckBox::GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept {
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

bool CheckBox::SetControlProperty(
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

AccessibleRole CheckBox::AccessibilityRole() const noexcept {
    return AccessibleRole::CheckBox;
}

std::wstring CheckBox::AccessibilityName() const {
    return label_;
}

bool CheckBox::HasAccessibilityToggle() const noexcept {
    return true;
}

bool CheckBox::AccessibilityToggleOn() const noexcept {
    return checked_;
}

void CheckBox::ToggleAccessibility() noexcept {
    if (IsEnabled()) {
        Toggle();
    }
}

}  // namespace nopeui
