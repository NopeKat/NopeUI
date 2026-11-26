#include "nopeui/controls/button.h"

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

#include "nopeui/graphics/text.h"
#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kPaddingX = 16.0f;
constexpr float kPaddingY = 8.0f;
constexpr float kMinWidth = 88.0f;
constexpr float kMinHeight = 32.0f;

}  // namespace

Ref<Button> Button::Create(std::wstring text) noexcept {
    Ref<Button> button(new Button());
    button->text_ = std::move(text);
    auto run = graphics::TextRun::Create(button->text_, 14.0f, false);
    if (run.ok()) {
        button->run_ = std::move(run.value());
    }
    return button;
}

Size Button::MeasureOverride(Size available) noexcept {
    const Size text = run_ != nullptr
        ? run_->Measure()
        : Size{static_cast<float>(text_.size()) * 8.0f, 20.0f};
    return Size{
        std::min(std::max(text.width + kPaddingX * 2.0f, kMinWidth), available.width),
        std::min(std::max(text.height + kPaddingY * 2.0f, kMinHeight), available.height)};
}

void Button::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    const std::uint32_t background = pressed_
        ? colors.button_pressed
        : (hovered_ ? colors.button_hover : colors.button);
    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(background), brush.GetAddressOf()))) {
        return;
    }
    target->FillRectangle(
        D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
        brush.Get());

    ComPtr<ID2D1SolidColorBrush> outline;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.button_border), outline.GetAddressOf()))) {
        target->DrawRectangle(
            D2D1::RectF(
                bounds_.x + 0.5f,
                bounds_.y + 0.5f,
                bounds_.Right() - 0.5f,
                bounds_.Bottom() - 0.5f),
            outline.Get(),
            1.0f);
    }

    if (run_ == nullptr) {
        return;
    }
    const Size text = run_->Measure();
    run_->Draw(
        target,
        Point{
            bounds_.x + (bounds_.width - text.width) / 2.0f,
            bounds_.y + (bounds_.height - text.height) / 2.0f},
        colors.text);

    if (IsFocused()) {
        ComPtr<ID2D1SolidColorBrush> focus;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(colors.accent), focus.GetAddressOf()))) {
            target->DrawRectangle(
                D2D1::RectF(
                    bounds_.x + 2.5f,
                    bounds_.y + 2.5f,
                    bounds_.Right() - 2.5f,
                    bounds_.Bottom() - 2.5f),
                focus.Get(),
                1.0f);
        }
    }
}

void Button::OnPointerEnter() noexcept {
    hovered_ = true;
}

void Button::OnPointerLeave() noexcept {
    hovered_ = false;
    pressed_ = false;
}

void Button::OnPointerDown(Point point) noexcept {
    if (Contains(point)) {
        pressed_ = true;
        hovered_ = true;
        Invalidate();
    }
}

void Button::OnPointerUp(Point point) noexcept {
    const bool clicked = pressed_ && Contains(point);
    pressed_ = false;
    Invalidate();
    if (clicked) {
        click_.Raise();
    }
}

bool Button::IsFocusable() const noexcept {
    return true;
}

bool Button::OnKeyDown(unsigned int key) noexcept {
    if (key != VK_SPACE && key != VK_RETURN) {
        return false;
    }
    if (!pressed_) {
        pressed_ = true;
        Invalidate();
    }
    return true;
}

bool Button::OnKeyUp(unsigned int key) noexcept {
    if (key != VK_SPACE && key != VK_RETURN) {
        return false;
    }
    const bool clicked = pressed_;
    pressed_ = false;
    Invalidate();
    if (clicked) {
        click_.Raise();
    }
    return true;
}

}  // namespace nopeui
