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
#include <cstring>
#include <iterator>
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

const PropertyMetadata kTextBoxProperties[] = {
    {L"Text", PropertyValueType::String, PropertyFlags::AffectsMeasure, PropertyValue(std::wstring())},
    {L"Placeholder", PropertyValueType::String, PropertyFlags::AffectsRender, PropertyValue(std::wstring())},
};

std::wstring ReadClipboardText() {
    if (OpenClipboard(nullptr) == FALSE) {
        return {};
    }
    std::wstring text;
    if (HANDLE handle = GetClipboardData(CF_UNICODETEXT); handle != nullptr) {
        const wchar_t* data = static_cast<const wchar_t*>(GlobalLock(handle));
        if (data != nullptr) {
            text.assign(data);
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    return text;
}

bool WriteClipboardText(const std::wstring& text) {
    if (text.empty() || OpenClipboard(nullptr) == FALSE) {
        return false;
    }
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
    bool written = false;
    if (handle != nullptr) {
        void* data = GlobalLock(handle);
        if (data != nullptr) {
            std::memcpy(data, text.c_str(), bytes);
            GlobalUnlock(handle);
            if (SetClipboardData(CF_UNICODETEXT, handle) != nullptr) {
                written = true;
            }
        }
        if (!written) {
            GlobalFree(handle);
        }
    }
    CloseClipboard();
    return written;
}

std::wstring ToSingleLine(std::wstring text) {
    for (wchar_t& character : text) {
        if (character == L'\r' || character == L'\n' || character == L'\t') {
            character = L' ';
        }
    }
    return text;
}

}  // namespace

Ref<TextBox> TextBox::Create(std::wstring text, graphics::TextStyle style) noexcept {
    Ref<TextBox> box(new TextBox());
    box->text_ = std::move(text);
    box->style_ = std::move(style);
    box->caret_ = box->text_.size();
    box->anchor_ = box->caret_;
    box->RebuildRun();
    return box;
}

void TextBox::SetText(std::wstring text) noexcept {
    text_ = std::move(text);
    caret_ = text_.size();
    anchor_ = caret_;
    scroll_x_ = 0.0f;
    ClearComposition();
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

void TextBox::RebuildCompositionRun() noexcept {
    composition_run_.reset();
    if (composition_.empty()) {
        return;
    }
    std::wstring display = text_;
    display.insert(caret_, composition_);
    auto created = graphics::TextRun::Create(display, style_, 8192.0f);
    if (created.ok()) {
        composition_run_ = std::move(created.value());
    }
}

void TextBox::ClearComposition() noexcept {
    composition_.clear();
    composition_run_.reset();
    composing_ = false;
}

void TextBox::DropSelection() noexcept {
    if (!HasSelection()) {
        return;
    }
    const std::size_t start = SelectionStart();
    text_.erase(start, SelectionEnd() - start);
    caret_ = start;
    anchor_ = start;
    RebuildRun();
    InvalidateLayout();
}

void TextBox::MoveCaret(std::size_t position, bool extend) noexcept {
    caret_ = std::min(position, text_.size());
    if (!extend) {
        anchor_ = caret_;
    }
    Invalidate();
}

std::size_t TextBox::SelectionStart() const noexcept {
    return std::min(caret_, anchor_);
}

std::size_t TextBox::SelectionEnd() const noexcept {
    return std::max(caret_, anchor_);
}

bool TextBox::HasSelection() const noexcept {
    return caret_ != anchor_;
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

void TextBox::EnsureCaretVisible() noexcept {
    const graphics::TextRun* display = composition_run_ != nullptr
        ? composition_run_.get()
        : run_.get();
    if (display == nullptr) {
        scroll_x_ = 0.0f;
        return;
    }
    const std::size_t index = composing_ ? caret_ + composition_.size() : caret_;
    const float caret_x = display->CaretX(index);
    const float viewport = std::max(1.0f, bounds_.width - kPaddingX * 2.0f);
    if (caret_x - scroll_x_ < 0.0f) {
        scroll_x_ = caret_x;
    } else if (caret_x - scroll_x_ > viewport) {
        scroll_x_ = caret_x - viewport + 2.0f;
    }
    scroll_x_ = std::max(0.0f, scroll_x_);
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

    EnsureCaretVisible();

    PushContentClip(target);

    const float text_x = bounds_.x + kPaddingX - scroll_x_;
    const graphics::TextRun* display = composition_run_ != nullptr
        ? composition_run_.get()
        : run_.get();
    float text_y = bounds_.y + kPaddingY;

    if (display != nullptr) {
        const Size text = display->Measure();
        text_y = bounds_.y + (bounds_.height - text.height) / 2.0f;

        if (!composing_ && run_ != nullptr && HasSelection()) {
            ComPtr<ID2D1SolidColorBrush> selection_brush;
            if (SUCCEEDED(target->CreateSolidColorBrush(
                    D2D1::ColorF(colors.accent, 0.28f),
                    selection_brush.GetAddressOf()))) {
                const std::size_t start = SelectionStart();
                const std::size_t end = SelectionEnd();
                for (std::size_t index = start; index < end; ++index) {
                    const float left = run_->CaretX(index);
                    const float right = run_->CaretX(index + 1);
                    if (right <= left) {
                        continue;
                    }
                    target->FillRectangle(
                        D2D1::RectF(
                            text_x + left,
                            text_y,
                            text_x + right,
                            text_y + text.height),
                        selection_brush.Get());
                }
            }
        }

        display->Draw(target, Point{text_x, text_y}, colors.text);

        if (composing_ && !composition_.empty()) {
            const float left = display->CaretX(caret_);
            const float right = display->CaretX(caret_ + composition_.size());
            ComPtr<ID2D1SolidColorBrush> underline;
            if (SUCCEEDED(target->CreateSolidColorBrush(
                    D2D1::ColorF(colors.accent), underline.GetAddressOf()))) {
                const float y = text_y + text.height - 1.0f;
                target->DrawLine(
                    D2D1::Point2F(text_x + left, y),
                    D2D1::Point2F(text_x + right, y),
                    underline.Get(),
                    1.5f);
            }
        }
    } else if (placeholder_run_ != nullptr) {
        const Size text = placeholder_run_->Measure();
        text_y = bounds_.y + (bounds_.height - text.height) / 2.0f;
        placeholder_run_->Draw(
            target,
            Point{text_x, text_y},
            colors.text_disabled);
    }

    if (focused) {
        const std::size_t index = composing_ ? caret_ + composition_.size() : caret_;
        const float caret_x = display != nullptr
            ? text_x + display->CaretX(index)
            : bounds_.x + kPaddingX;
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
    dragging_ = false;
    Invalidate();
}

void TextBox::OnPointerDown(Point point) noexcept {
    if (!IsEnabled()) {
        return;
    }
    ClearComposition();
    if (run_ == nullptr) {
        caret_ = 0;
        anchor_ = 0;
        dragging_ = true;
        Invalidate();
        return;
    }
    const float local_x = point.x - (bounds_.x + kPaddingX) + scroll_x_;
    const std::size_t index = run_->IndexFromX(std::max(0.0f, local_x));
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    caret_ = index;
    if (!shift) {
        anchor_ = index;
    }
    dragging_ = true;
    Invalidate();
}

void TextBox::OnPointerMove(Point point) noexcept {
    if (!dragging_ || run_ == nullptr) {
        return;
    }
    const float local_x = point.x - (bounds_.x + kPaddingX) + scroll_x_;
    caret_ = run_->IndexFromX(std::max(0.0f, local_x));
    Invalidate();
}

void TextBox::OnPointerUp(Point) noexcept {
    dragging_ = false;
}

bool TextBox::IsFocusable() const noexcept {
    return IsEnabled();
}

bool TextBox::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled()) {
        return false;
    }
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (ctrl) {
        switch (key) {
            case 'A':
                anchor_ = 0;
                caret_ = text_.size();
                Invalidate();
                return true;
            case 'C':
                if (HasSelection()) {
                    WriteClipboardText(
                        text_.substr(SelectionStart(), SelectionEnd() - SelectionStart()));
                }
                return true;
            case 'X':
                if (HasSelection()) {
                    WriteClipboardText(
                        text_.substr(SelectionStart(), SelectionEnd() - SelectionStart()));
                    DropSelection();
                }
                return true;
            case 'V': {
                const std::wstring pasted = ToSingleLine(ReadClipboardText());
                if (pasted.empty()) {
                    return true;
                }
                DropSelection();
                text_.insert(caret_, pasted);
                caret_ += pasted.size();
                anchor_ = caret_;
                RebuildRun();
                InvalidateLayout();
                return true;
            }
            default:
                break;
        }
    }

    switch (key) {
        case VK_BACK:
            if (HasSelection()) {
                DropSelection();
            } else if (caret_ > 0) {
                text_.erase(caret_ - 1, 1);
                --caret_;
                anchor_ = caret_;
                RebuildRun();
                InvalidateLayout();
            }
            return true;
        case VK_DELETE:
            if (HasSelection()) {
                DropSelection();
            } else if (caret_ < text_.size()) {
                text_.erase(caret_, 1);
                RebuildRun();
                InvalidateLayout();
            }
            return true;
        case VK_LEFT:
            if (HasSelection() && !shift) {
                MoveCaret(SelectionStart(), false);
            } else {
                MoveCaret(caret_ > 0 ? caret_ - 1 : 0, shift);
            }
            return true;
        case VK_RIGHT:
            if (HasSelection() && !shift) {
                MoveCaret(SelectionEnd(), false);
            } else {
                MoveCaret(caret_ + 1, shift);
            }
            return true;
        case VK_HOME:
            MoveCaret(0, shift);
            return true;
        case VK_END:
            MoveCaret(text_.size(), shift);
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
    if (HasSelection()) {
        DropSelection();
    }
    text_.insert(caret_, 1, character);
    ++caret_;
    anchor_ = caret_;
    RebuildRun();
    InvalidateLayout();
    return true;
}

void TextBox::OnImeStart() noexcept {
    composing_ = true;
}

void TextBox::OnImeComposition(std::wstring_view text) noexcept {
    composing_ = true;
    if (text.empty()) {
        composition_.clear();
        composition_run_.reset();
        Invalidate();
        return;
    }
    if (composition_.empty() && HasSelection()) {
        DropSelection();
    }
    composition_.assign(text);
    RebuildCompositionRun();
    Invalidate();
}

void TextBox::OnImeResult(std::wstring_view text) noexcept {
    if (text.empty()) {
        ClearComposition();
        Invalidate();
        return;
    }
    if (HasSelection()) {
        DropSelection();
    }
    const std::wstring commit(text);
    text_.insert(caret_, commit);
    caret_ += commit.size();
    anchor_ = caret_;
    ClearComposition();
    RebuildRun();
    InvalidateLayout();
}

void TextBox::OnImeEnd() noexcept {
    ClearComposition();
    Invalidate();
}

Point TextBox::ImeCaretPoint() const noexcept {
    const graphics::TextRun* display = composition_run_ != nullptr
        ? composition_run_.get()
        : run_.get();
    float caret_x = bounds_.x + kPaddingX - scroll_x_;
    if (display != nullptr) {
        const std::size_t index = composing_ ? caret_ + composition_.size() : caret_;
        caret_x += display->CaretX(index);
    }
    return Point{caret_x, bounds_.Bottom() - kPaddingY};
}

const PropertyMetadata* TextBox::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kTextBoxProperties);
    }
    return kTextBoxProperties;
}

bool TextBox::GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept {
    if (name == L"Text") {
        *value = PropertyValue(text_);
        return true;
    }
    if (name == L"Placeholder") {
        *value = PropertyValue(placeholder_);
        return true;
    }
    return false;
}

bool TextBox::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    std::wstring text;
    if (!value.AsString(&text)) {
        return false;
    }
    if (name == L"Text") {
        SetText(std::move(text));
        return true;
    }
    if (name == L"Placeholder") {
        SetPlaceholder(std::move(text));
        return true;
    }
    return false;
}

AccessibleRole TextBox::AccessibilityRole() const noexcept {
    return AccessibleRole::Edit;
}

std::wstring TextBox::AccessibilityName() const {
    return placeholder_.empty() ? L"Text" : placeholder_;
}

std::wstring TextBox::AccessibilityValue() const {
    return text_;
}

bool TextBox::SetAccessibilityValue(std::wstring_view value) {
    if (!IsEnabled()) {
        return false;
    }
    SetText(std::wstring(value));
    return true;
}

bool TextBox::HasAccessibilityValue() const noexcept {
    return true;
}

bool TextBox::HasAccessibilityText() const noexcept {
    return true;
}

std::wstring TextBox::AccessibilityText() const {
    return text_;
}

int TextBox::AccessibilitySelectionStart() const noexcept {
    return static_cast<int>(SelectionStart());
}

int TextBox::AccessibilitySelectionEnd() const noexcept {
    return static_cast<int>(SelectionEnd());
}

bool TextBox::SetAccessibilitySelection(int start, int end) {
    const int length = static_cast<int>(text_.size());
    int first = std::clamp(start, 0, length);
    int last = std::clamp(end, 0, length);
    if (first > last) {
        std::swap(first, last);
    }
    anchor_ = static_cast<std::size_t>(first);
    caret_ = static_cast<std::size_t>(last);
    EnsureCaretVisible();
    Invalidate();
    return true;
}

}  // namespace nopeui
