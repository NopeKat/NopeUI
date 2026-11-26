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
    const std::uint32_t background = pressed_ ? 0xD4D4D4 : (hovered_ ? 0xE2E2E2 : 0xEBEBEB);
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
            D2D1::ColorF(0xCFCFCF), outline.GetAddressOf()))) {
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
        0x1B1B1B);
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
    }
}

void Button::OnPointerUp(Point) noexcept {
    pressed_ = false;
}

}  // namespace nopeui
