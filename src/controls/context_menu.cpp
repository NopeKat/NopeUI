#include "nopeui/controls/context_menu.h"

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

constexpr float kItemHeight = 30.0f;
constexpr float kPaddingX = 10.0f;
constexpr float kPaddingY = 6.0f;
constexpr float kMinWidth = 180.0f;
constexpr float kCornerRadius = 6.0f;
constexpr std::uint32_t kNoItem = 0xffffffffu;

}  // namespace

Ref<ContextMenu> ContextMenu::Create() noexcept {
    Ref<ContextMenu> menu(new ContextMenu());
    menu->SetClosePopupOnClick(true);
    return menu;
}

void ContextMenu::AddItem(std::wstring text, std::function<void()> handler) noexcept {
    Item item;
    item.text = std::move(text);
    graphics::TextStyle style;
    style.size = 13.0f;
    auto run = graphics::TextRun::Create(item.text, style);
    if (run.ok()) {
        item.run = std::move(run.value());
    }
    item.handler = std::move(handler);
    items_.push_back(std::move(item));
    InvalidateLayout();
}

Size ContextMenu::MeasureOverride(Size available) noexcept {
    float width = kMinWidth;
    float height = kPaddingY * 2.0f;
    for (const auto& item : items_) {
        const float item_width = item.run != nullptr
            ? item.run->Measure().width + kPaddingX * 2.0f
            : static_cast<float>(item.text.size()) * 7.0f + kPaddingX * 2.0f;
        width = std::max(width, item_width);
        height += kItemHeight;
    }
    return Size{
        std::min(width, available.width),
        std::min(height, available.height)};
}

void ContextMenu::PaintContent(ID2D1RenderTarget* target) noexcept {
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

    ComPtr<ID2D1SolidColorBrush> hover;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.button_hover), hover.GetAddressOf()))) {
        for (std::uint32_t index = 0; index < items_.size(); ++index) {
            if (index != hovered_) {
                continue;
            }
            const float top = bounds_.y + kPaddingY + kItemHeight * static_cast<float>(index);
            target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(
                        bounds_.x + 4.0f,
                        top + 1.0f,
                        bounds_.Right() - 4.0f,
                        top + kItemHeight - 1.0f),
                    3.0f,
                    3.0f),
                hover.Get());
        }
    }

    ComPtr<ID2D1SolidColorBrush> text;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.text), text.GetAddressOf()))) {
        for (std::uint32_t index = 0; index < items_.size(); ++index) {
            const Item& item = items_[index];
            if (item.run == nullptr) {
                continue;
            }
            const Size measured = item.run->Measure();
            const float top = bounds_.y + kPaddingY + kItemHeight * static_cast<float>(index);
            item.run->Draw(
                target,
                Point{
                    bounds_.x + kPaddingX,
                    top + (kItemHeight - measured.height) / 2.0f},
                colors.text);
        }
    }
}

int ContextMenu::ItemAt(Point point) const noexcept {
    if (!Contains(point)) {
        return -1;
    }
    const float local_y = point.y - bounds_.y - kPaddingY;
    if (local_y < 0.0f) {
        return -1;
    }
    const std::uint32_t index =
        static_cast<std::uint32_t>(local_y / kItemHeight);
    if (index >= items_.size()) {
        return -1;
    }
    return static_cast<int>(index);
}

void ContextMenu::OnPointerEnter() noexcept {
    hovered_ = kNoItem;
}

void ContextMenu::OnPointerLeave() noexcept {
    hovered_ = kNoItem;
    pressed_ = false;
    Invalidate();
}

void ContextMenu::OnPointerMove(Point point) noexcept {
    int index = ItemAt(point);
    const std::uint32_t hovered = index < 0 ? kNoItem : static_cast<std::uint32_t>(index);
    if (hovered != hovered_) {
        hovered_ = hovered;
        Invalidate();
    }
}

void ContextMenu::OnPointerDown(Point point) noexcept {
    pressed_ = ItemAt(point) >= 0;
    Invalidate();
}

void ContextMenu::OnPointerUp(Point point) noexcept {
    const int index = ItemAt(point);
    const bool activate = pressed_ && index >= 0;
    pressed_ = false;
    Invalidate();
    if (!activate) {
        return;
    }
    auto handler = items_[static_cast<std::size_t>(index)].handler;
    if (handler) {
        handler();
    }
}

AccessibleRole ContextMenu::AccessibilityRole() const noexcept {
    return AccessibleRole::Pane;
}

std::wstring ContextMenu::AccessibilityName() const {
    return L"Context menu";
}

}  // namespace nopeui
