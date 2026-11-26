#include "nopeui/controls/flyout.h"

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

#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kPaddingX = 16.0f;
constexpr float kPaddingY = 14.0f;
constexpr float kCornerRadius = 6.0f;

}  // namespace

Ref<Flyout> Flyout::Create() noexcept {
    return Ref<Flyout>(new Flyout());
}

Flyout::~Flyout() = default;

void Flyout::SetContent(Ref<Element> content) noexcept {
    if (content_) {
        content_->SetParent(nullptr);
    }
    content_ = std::move(content);
    if (content_) {
        content_->SetParent(this);
    }
    InvalidateLayout();
}

Size Flyout::MeasureOverride(Size available) noexcept {
    const float inner_width = std::max(0.0f, available.width - kPaddingX * 2.0f);
    const float inner_height = std::max(0.0f, available.height - kPaddingY * 2.0f);
    if (!content_) {
        return Size{
            std::min(160.0f, available.width),
            std::min(64.0f, available.height)};
    }
    content_->Measure(Size{inner_width, inner_height});
    const Size measured = content_->DesiredSize();
    return Size{
        std::min(measured.width + kPaddingX * 2.0f, available.width),
        std::min(measured.height + kPaddingY * 2.0f, available.height)};
}

Size Flyout::ArrangeOverride(Size final_size) noexcept {
    if (content_) {
        content_->Arrange(Rect{
            bounds_.x + kPaddingX,
            bounds_.y + kPaddingY,
            std::max(0.0f, final_size.width - kPaddingX * 2.0f),
            std::max(0.0f, final_size.height - kPaddingY * 2.0f)});
    }
    return final_size;
}

void Flyout::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    ComPtr<ID2D1SolidColorBrush> background;
    if (FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.surface), background.GetAddressOf()))) {
        return;
    }
    target->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
            kCornerRadius,
            kCornerRadius),
        background.Get());

    ComPtr<ID2D1SolidColorBrush> outline;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.surface_border), outline.GetAddressOf()))) {
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
    if (content_) {
        PushContentClip(target);
        content_->Paint(target);
        PopContentClip(target);
    }
}

void Flyout::CollectChildren(std::vector<Element*>& elements) noexcept {
    if (content_) {
        elements.push_back(content_.Get());
    }
}

AccessibleRole Flyout::AccessibilityRole() const noexcept {
    return AccessibleRole::Group;
}

}  // namespace nopeui
