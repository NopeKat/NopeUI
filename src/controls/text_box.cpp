#include "nopeui/controls/text_box.h"

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

constexpr float kPaddingX = 10.0f;
constexpr float kPaddingY = 8.0f;
constexpr float kMinHeight = 32.0f;
constexpr float kCornerRadius = 4.0f;

}  // namespace

Ref<TextBox> TextBox::Create(std::wstring text, graphics::TextStyle style) noexcept {
    Ref<TextBox> box(new TextBox());
    box->text_ = std::move(text);
    box->style_ = std::move(style);
    box->caret_ = box->text_.size();
    box->RebuildRun();
    return box;
}

void TextBox::SetText(std::wstring text) noexcept {
    text_ = std::move(text);
    caret_ = text_.size();
    RebuildRun();
    InvalidateLayout();
}

void TextBox::SetPlaceholder(std::wstring placeholder) noexcept {
    placeholder_ = std::move(placeholder);
    RebuildPlaceholder();
    Invalidate();
}

void TextBox::RebuildRun() noexcept {
    run_.reset();
    if (text_.empty()) {
        return;
    }
    auto created = graphics::TextRun::Create(text_, style_, 8192.0f);
    if (created.ok()) {
        run_ = std::move(created.value());
    }
}

void TextBox::RebuildPlaceholder() noexcept {
    placeholder_run_.reset();
    if (placeholder_.empty()) {
        return;
    }
    graphics::TextStyle style = style_;
    style.ellipsis = false;
    auto created = graphics::TextRun::Create(placeholder_, style, 8192.0f);
    if (created.ok()) {
        placeholder_run_ = std::move(created.value());
    }
}

Size TextBox::MeasureOverride(Size available) noexcept {
    float text_height = style_.size + 4.0f;
    if (run_ != nullptr) {
        text_height = run_->Measure().height;
    }
    return Size{
        available.width,
        std::min(std::max(text_height + kPaddingY * 2.0f, kMinHeight), available.height)};
}

void TextBox::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();

    std::uint32_t background = colors.surface;
    if (!IsEnabled()) {
        background = colors.button_disabled;
    } else if (hovered_) {
        background = colors.button_hover;
    }
    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(background), brush.GetAddressOf()))) {
        return;
    }
    target->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
            kCornerRadius,
            kCornerRadius),
        brush.Get());

    const bool focused = IsFocused() && IsEnabled();
    const std::uint32_t border = !IsEnabled()
        ? colors.button_disabled_border
        : (focused ? colors.accent : colors.surface_border);
    ComPtr<ID2D1SolidColorBrush> outline;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(border), outline.GetAddressOf()))) {
        const float inset = focused ? 1.0f : 0.5f;
        target->DrawRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(
                    bounds_.x + inset,
                    bounds_.y + inset,
                    bounds_.Right() - inset,
                    bounds_.Bottom() - inset),
                kCornerRadius,
                kCornerRadius),
            outline.Get(),
            focused ? 2.0f : 1.0f);
    }

    PushContentClip(target);

    const float text_x = bounds_.x + kPaddingX;
    if (run_ != nullptr) {
        const Size text = run_->Measure();
        const float text_y = bounds_.y + (bounds_.height - text.height) / 2.0f;
        run_->Draw(target, Point{text_x, text_y}, colors.text);
    } else if (placeholder_run_ != nullptr) {
        const Size text = placeholder_run_->Measure();
        const float text_y = bounds_.y + (bounds_.height - text.height) / 2.0f;
        placeholder_run_->Draw(
            target,
            Point{text_x, text_y},
            colors.text_disabled);
    }

    if (focused) {
        const float caret_x = run_ != nullptr
            ? text_x + run_->CaretX(caret_)
            : text_x;
        const float caret_top = bounds_.y + kPaddingY;
        const float caret_bottom = bounds_.Bottom() - kPaddingY;
        ComPtr<ID2D1SolidColorBrush> caret_brush;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(colors.accent), caret_brush.GetAddressOf()))) {
            target->DrawLine(
                D2D1::Point2F(caret_x, caret_top),
                D2D1::Point2F(caret_x, caret_bottom),
                caret_brush.Get(),
                1.5f);
        }
    }

    PopContentClip(target);
}

void TextBox::OnPointerEnter() noexcept {
    hovered_ = true;
    Invalidate();
}

void TextBox::OnPointerLeave() noexcept {
    hovered_ = false;
    Invalidate();
}

void TextBox::OnPointerDown(Point point) noexcept {
    if (!IsEnabled() || run_ == nullptr) {
        return;
    }
    const float local_x = point.x - (bounds_.x + kPaddingX);
    caret_ = run_->IndexFromX(local_x);
    Invalidate();
}

bool TextBox::IsFocusable() const noexcept {
    return IsEnabled();
}

bool TextBox::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled()) {
        return false;
    }
    switch (key) {
        case VK_BACK:
            if (caret_ > 0) {
                text_.erase(caret_ - 1, 1);
                --caret_;
                RebuildRun();
                InvalidateLayout();
            }
            return true;
        case VK_DELETE:
            if (caret_ < text_.size()) {
                text_.erase(caret_, 1);
                RebuildRun();
                InvalidateLayout();
            }
            return true;
        case VK_LEFT:
            if (caret_ > 0) {
                --caret_;
                Invalidate();
            }
            return true;
        case VK_RIGHT:
            if (caret_ < text_.size()) {
                ++caret_;
                Invalidate();
            }
            return true;
        case VK_HOME:
            caret_ = 0;
            Invalidate();
            return true;
        case VK_END:
            caret_ = text_.size();
            Invalidate();
            return true;
        case VK_RETURN:
            if (submit_) {
                submit_();
            }
            return true;
        default:
            return false;
    }
}

bool TextBox::OnChar(wchar_t character) noexcept {
    if (!IsEnabled()) {
        return false;
    }
    if (character < 0x20 || character == 0x7F) {
        return false;
    }
    text_.insert(caret_, 1, character);
    ++caret_;
    RebuildRun();
    InvalidateLayout();
    return true;
}

}  // namespace nopeui
