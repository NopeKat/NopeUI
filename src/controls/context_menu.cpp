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
    target->PushAxisAlignedClip(
        D2D1::RectF(
            bounds_.x,
            bounds_.y + kPaddingY,
            bounds_.Right(),
            bounds_.Bottom() - kPaddingY),
        D2D1_ANTIALIAS_MODE_ALIASED);
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.button_hover), hover.GetAddressOf()))) {
        for (std::uint32_t index = 0; index < items_.size(); ++index) {
            if (index != hovered_) {
                continue;
            }
            const float top =
                bounds_.y + kPaddingY + kItemHeight * static_cast<float>(index) - scroll_;
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
            const float top =
                bounds_.y + kPaddingY + kItemHeight * static_cast<float>(index) - scroll_;
            item.run->Draw(
                target,
                Point{
                    bounds_.x + kPaddingX,
                    top + (kItemHeight - measured.height) / 2.0f},
                colors.text);
        }
    }
    PopContentClip(target);
}

float ContextMenu::ContentHeight() const noexcept {
    return kPaddingY * 2.0f + kItemHeight * static_cast<float>(items_.size());
}

float ContextMenu::MaxScroll() const noexcept {
    return std::max(0.0f, ContentHeight() - bounds_.height);
}

void ContextMenu::EnsureVisible(int index) noexcept {
    if (index < 0) {
        return;
    }
    const float top = kItemHeight * static_cast<float>(index);
    const float visible = bounds_.height - kPaddingY * 2.0f;
    if (top < scroll_) {
        scroll_ = top;
    } else if (top + kItemHeight > scroll_ + visible) {
        scroll_ = top + kItemHeight - visible;
    }
    scroll_ = std::clamp(scroll_, 0.0f, MaxScroll());
}

void ContextMenu::Activate(int index) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= items_.size()) {
        return;
    }
    auto handler = items_[static_cast<std::size_t>(index)].handler;
    // Close first so a command that opens another popup is not dismissed by
    // the pending close of this menu.
    RequestClosePopup();
    if (handler) {
        handler();
    }
}

bool ContextMenu::IsFocusable() const noexcept {
    return IsEnabled();
}

void ContextMenu::OnPopupOpened() noexcept {
    scroll_ = 0.0f;
    hovered_ = kNoItem;
    pressed_ = false;
    Invalidate();
}

bool ContextMenu::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled()) {
        return false;
    }
    const int count = static_cast<int>(items_.size());
    if (count == 0) {
        return false;
    }
    int index = hovered_ == kNoItem ? -1 : static_cast<int>(hovered_);
    switch (key) {
        case VK_DOWN:
            index = index < 0 ? 0 : std::min(index + 1, count - 1);
            break;
        case VK_UP:
            index = index < 0 ? 0 : std::max(index - 1, 0);
            break;
        case VK_HOME:
            index = 0;
            break;
        case VK_END:
            index = count - 1;
            break;
        case VK_RETURN:
        case VK_SPACE:
            Activate(index);
            return true;
        default:
            return false;
    }
    hovered_ = static_cast<std::uint32_t>(index);
    EnsureVisible(index);
    Invalidate();
    return true;
}

bool ContextMenu::OnWheel(int delta) noexcept {
    const float step = kItemHeight * 3.0f;
    const float next = std::clamp(
        scroll_ - (static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA)) * step,
        0.0f,
        MaxScroll());
    if (next != scroll_) {
        scroll_ = next;
        Invalidate();
    }
    return true;
}

int ContextMenu::ItemAt(Point point) const noexcept {
    if (!Contains(point)) {
        return -1;
    }
    const float local_y = point.y - bounds_.y - kPaddingY + scroll_;
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
    if (activate) {
        Activate(index);
    }
}

AccessibleRole ContextMenu::AccessibilityRole() const noexcept {
    return AccessibleRole::Menu;
}

std::wstring ContextMenu::AccessibilityName() const {
    return L"Context menu";
}

Rect ContextMenu::AccessibilityMenuItemBounds(std::size_t index) const noexcept {
    if (index >= items_.size()) {
        return Rect{};
    }
    const float top = kPaddingY + kItemHeight * static_cast<float>(index) - scroll_;
    const float visible_top = kPaddingY;
    const float visible_bottom =
        std::max(kPaddingY, bounds_.height - kPaddingY);
    const float clipped_top = std::max(top, visible_top);
    const float clipped_bottom = std::min(top + kItemHeight, visible_bottom);
    if (clipped_bottom <= clipped_top) {
        return Rect{};
    }
    return Rect{
        4.0f,
        clipped_top,
        std::max(0.0f, bounds_.width - 8.0f),
        clipped_bottom - clipped_top};
}

bool ContextMenu::InvokeAccessibilityMenuItem(std::size_t index) noexcept {
    if (index >= items_.size()) {
        return false;
    }
    Activate(static_cast<int>(index));
    return true;
}

void ContextMenu::CollectAccessibleMenuItems(
    std::vector<std::wstring>& items) const noexcept {
    for (const Item& item : items_) {
        items.push_back(item.text);
    }
}

}  // namespace nopeui
