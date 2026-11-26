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

#include "nopeui/theme/theme.h"

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

void Border::SetCornerRadius(float radius) noexcept {
    if (corner_radius_.Set(std::max(0.0f, radius))) {
        Invalidate();
    }
}

void Border::UseSurfaceBackground(bool surface) noexcept {
    if (surface_ == surface && use_theme_background_) {
        return;
    }
    surface_ = surface;
    use_theme_background_ = true;
    Invalidate();
}

void Border::SetBackground(std::uint32_t color) noexcept {
    use_theme_background_ = false;
    if (background_.Set(color)) {
        Invalidate();
    }
}

void Border::SetBorderColor(std::uint32_t color) noexcept {
    if (border_color_.Set(color)) {
        Invalidate();
    }
}

void Border::SetBorderVisible(bool visible) noexcept {
    if (border_visible_ == visible) {
        return;
    }
    border_visible_ = visible;
    Invalidate();
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
    const Theme& colors = Colors();
    const float radius = corner_radius_.Get();
    const std::uint32_t background = use_theme_background_
        ? (surface_ ? colors.surface : colors.background)
        : background_.Get();
    ComPtr<ID2D1SolidColorBrush> background_brush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(background), background_brush.GetAddressOf()))) {
        if (radius > 0.0f) {
            target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
                    radius,
                    radius),
                background_brush.Get());
        } else {
            target->FillRectangle(
                D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
                background_brush.Get());
        }
    }
    if (border_visible_) {
        const std::uint32_t color =
            border_color_.Get() != 0 ? border_color_.Get() : colors.surface_border;
        ComPtr<ID2D1SolidColorBrush> outline;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(color), outline.GetAddressOf()))) {
            const D2D1_RECT_F outline_rect = D2D1::RectF(
                bounds_.x + 0.5f,
                bounds_.y + 0.5f,
                bounds_.Right() - 0.5f,
                bounds_.Bottom() - 0.5f);
            if (radius > 0.0f) {
                const float outline_radius = std::max(0.0f, radius - 0.5f);
                target->DrawRoundedRectangle(
                    D2D1::RoundedRect(outline_rect, outline_radius, outline_radius),
                    outline.Get(),
                    kOutlineWidth);
            } else {
                target->DrawRectangle(outline_rect, outline.Get(), kOutlineWidth);
            }
        }
    }
    if (child_) {
        PushContentClip(target);
        child_->Paint(target);
        PopContentClip(target);
    }
}

Element* Border::HitTestOverride(Point point) noexcept {
    if (child_) {
        return child_->HitTest(point);
    }
    return nullptr;
}

void Border::CollectFocusable(std::vector<Element*>& elements) noexcept {
    if (!IsEnabled()) {
        return;
    }
    if (child_) {
        child_->CollectFocusable(elements);
    }
}

}  // namespace nopeui
