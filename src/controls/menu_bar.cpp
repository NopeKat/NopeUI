#include "nopeui/controls/menu_bar.h"

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
#include <cmath>

#include "nopeui/controls/context_menu.h"
#include "nopeui/graphics/text.h"
#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kPaddingX = 12.0f;
constexpr float kMinHeight = 30.0f;
constexpr float kTextSize = 13.0f;

}  // namespace

Ref<MenuBar> MenuBar::Create() noexcept {
    return Ref<MenuBar>(new MenuBar());
}

void MenuBar::AddMenu(std::wstring title, std::vector<MenuCommand> commands) {
    Item item;
    item.title = std::move(title);
    graphics::TextStyle style;
    style.size = kTextSize;
    auto run = graphics::TextRun::Create(item.title, style);
    if (run.ok()) {
        item.run = std::move(run.value());
    }
    item.commands = std::move(commands);
    items_.push_back(std::move(item));
    InvalidateLayout();
}

Size MenuBar::MeasureOverride(Size available) noexcept {
    float width = 0.0f;
    float height = 0.0f;
    for (const auto& item : items_) {
        const float text_width = item.run != nullptr
            ? item.run->Measure().width
            : static_cast<float>(item.title.size()) * 7.0f;
        width += text_width + kPaddingX * 2.0f;
        height = std::max(height, kMinHeight);
    }
    return Size{
        std::min(width, available.width),
        std::min(height, available.height)};
}

Size MenuBar::ArrangeOverride(Size final_size) noexcept {
    return final_size;
}

int MenuBar::ItemAt(Point point) const noexcept {
    if (!Contains(point)) {
        return -1;
    }
    float x = bounds_.x;
    for (std::size_t index = 0; index < items_.size(); ++index) {
        const auto& item = items_[index];
        const float text_width = item.run != nullptr
            ? item.run->Measure().width
            : static_cast<float>(item.title.size()) * 7.0f;
        const float width = text_width + kPaddingX * 2.0f;
        if (point.x >= x && point.x < x + width) {
            return static_cast<int>(index);
        }
        x += width;
    }
    return -1;
}

void MenuBar::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.surface), brush.GetAddressOf()))) {
        return;
    }
    target->FillRectangle(
        D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
        brush.Get());

    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.surface_border), brush.GetAddressOf()))) {
        target->FillRectangle(
            D2D1::RectF(
                bounds_.x, bounds_.Bottom() - 1.0f, bounds_.Right(), bounds_.Bottom()),
            brush.Get());
    }

    float x = bounds_.x;
    ComPtr<ID2D1SolidColorBrush> highlight;
    if (FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.button_hover), highlight.GetAddressOf())) ||
        FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.text), brush.GetAddressOf()))) {
        return;
    }

    for (std::size_t index = 0; index < items_.size(); ++index) {
        const auto& item = items_[index];
        const float text_width = item.run != nullptr
            ? item.run->Measure().width
            : static_cast<float>(item.title.size()) * 7.0f;
        const float width = text_width + kPaddingX * 2.0f;
        const int item_index = static_cast<int>(index);
        if (item_index == hovered_ || item_index == active_) {
            target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(
                        x + 2.0f,
                        bounds_.y + 2.0f,
                        x + width - 2.0f,
                        bounds_.Bottom() - 3.0f),
                    3.0f,
                    3.0f),
                highlight.Get());
        }
        if (item.run != nullptr) {
            const Size measured = item.run->Measure();
            ComPtr<ID2D1SolidColorBrush> text;
            const std::uint32_t color =
                item_index == active_ ? colors.accent : colors.text;
            if (SUCCEEDED(target->CreateSolidColorBrush(
                    D2D1::ColorF(color), text.GetAddressOf()))) {
                item.run->Draw(
                    target,
                    Point{
                        x + kPaddingX,
                        bounds_.y + (bounds_.height - measured.height) / 2.0f},
                    color);
            }
        }
        x += width;
    }
}

void MenuBar::OnPointerMove(Point point) noexcept {
    const int index = ItemAt(point);
    if (index != hovered_) {
        hovered_ = index;
        Invalidate();
    }
}

void MenuBar::OnPointerLeave() noexcept {
    if (hovered_ >= 0) {
        hovered_ = -1;
        Invalidate();
    }
    pressed_ = -1;
}

void MenuBar::OnPointerDown(Point point) noexcept {
    pressed_ = ItemAt(point);
    if (pressed_ >= 0) {
        hovered_ = pressed_;
        Invalidate();
    }
}

void MenuBar::OnPointerUp(Point point) noexcept {
    const int index = ItemAt(point);
    const bool activate = pressed_ >= 0 && index == pressed_;
    pressed_ = -1;
    if (!activate) {
        return;
    }
    // A second click on the open title closes its menu.
    if (active_ == index) {
        RequestClosePopup();
        active_ = -1;
        Invalidate();
        return;
    }
    OpenMenu(static_cast<std::size_t>(index));
}

void MenuBar::OpenMenu(std::size_t index) noexcept {
    if (index >= items_.size()) {
        return;
    }
    active_ = static_cast<int>(index);
    Invalidate();

    float x = bounds_.x;
    for (std::size_t offset = 0; offset < index; ++offset) {
        const auto& item = items_[offset];
        const float text_width = item.run != nullptr
            ? item.run->Measure().width
            : static_cast<float>(item.title.size()) * 7.0f;
        x += text_width + kPaddingX * 2.0f;
    }

    auto menu = ContextMenu::Create();
    for (const MenuCommand& command : items_[index].commands) {
        menu->AddItem(command.text, command.handler);
    }
    RequestPopup(menu, Point{x, bounds_.Bottom()});
}

bool MenuBar::IsFocusable() const noexcept {
    return IsEnabled() && !items_.empty();
}

bool MenuBar::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled() || items_.empty()) {
        return false;
    }
    const int count = static_cast<int>(items_.size());
    if (key == VK_LEFT || key == VK_RIGHT) {
        const int step = key == VK_RIGHT ? 1 : -1;
        active_ = (active_ < 0) ? (step > 0 ? 0 : count - 1)
                                : (active_ + step + count) % count;
        hovered_ = active_;
        Invalidate();
        return true;
    }
    if (key == VK_DOWN || key == VK_RETURN) {
        OpenMenu(static_cast<std::size_t>(active_ < 0 ? 0 : active_));
        return true;
    }
    if (key == VK_ESCAPE && active_ >= 0) {
        RequestClosePopup();
        active_ = -1;
        Invalidate();
        return true;
    }
    return false;
}

AccessibleRole MenuBar::AccessibilityRole() const noexcept {
    return AccessibleRole::MenuBar;
}

std::wstring MenuBar::AccessibilityName() const {
    return L"Menu bar";
}

}  // namespace nopeui
