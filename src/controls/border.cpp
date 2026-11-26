#include "nopeui/controls/border.h"

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

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kOutlineWidth = 1.0f;

float InsetWidth(const Thickness& padding) noexcept {
    return padding.Horizontal() + kOutlineWidth * 2.0f;
}

}  // namespace

Ref<Border> Border::Create() noexcept {
    return Ref<Border>(new Border());
}

void Border::SetChild(Ref<Element> child) noexcept {
    child_ = std::move(child);
    if (child_) {
        child_->SetParent(this);
    }
}

void Border::SetPadding(Thickness padding) noexcept {
    padding_ = padding;
}

void Border::SetBackground(std::uint32_t color) noexcept {
    background_ = color;
}

void Border::SetBorderColor(std::uint32_t color) noexcept {
    border_color_ = color;
}

Size Border::MeasureOverride(Size available) noexcept {
    if (!child_) {
        return Size{
            padding_.Horizontal() + kOutlineWidth * 2.0f,
            padding_.Vertical() + kOutlineWidth * 2.0f};
    }
    const Size inner{
        std::max(0.0f, available.width - InsetWidth(padding_)),
        std::max(0.0f, available.height - padding_.Vertical() - kOutlineWidth * 2.0f)};
    child_->Measure(inner);
    const Size child_size = child_->DesiredSize();
    return Size{
        child_size.width + InsetWidth(padding_),
        child_size.height + padding_.Vertical() + kOutlineWidth * 2.0f};
}

Size Border::ArrangeOverride(Size final_size) noexcept {
    if (child_) {
        child_->Arrange(Rect{
            bounds_.x + padding_.left + kOutlineWidth,
            bounds_.y + padding_.top + kOutlineWidth,
            std::max(0.0f, final_size.width - InsetWidth(padding_)),
            std::max(0.0f, final_size.height - padding_.Vertical() - kOutlineWidth * 2.0f)});
    }
    return final_size;
}

void Border::PaintContent(ID2D1RenderTarget* target) noexcept {
    ComPtr<ID2D1SolidColorBrush> background;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(background_), background.GetAddressOf()))) {
        target->FillRectangle(
            D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
            background.Get());
    }
    if (border_color_ != 0) {
        ComPtr<ID2D1SolidColorBrush> outline;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(border_color_), outline.GetAddressOf()))) {
            target->DrawRectangle(
                D2D1::RectF(
                    bounds_.x + 0.5f,
                    bounds_.y + 0.5f,
                    bounds_.Right() - 0.5f,
                    bounds_.Bottom() - 0.5f),
                outline.Get(),
                kOutlineWidth);
        }
    }
    if (child_) {
        child_->Paint(target);
    }
}

Element* Border::HitTestOverride(Point point) noexcept {
    if (child_) {
        return child_->HitTest(point);
    }
    return nullptr;
}

}  // namespace nopeui
