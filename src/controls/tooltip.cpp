#include "nopeui/controls/tooltip.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>
#include <wrl/client.h>

#include <unordered_map>
#include <utility>

#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kPaddingX = 9.0f;
constexpr float kPaddingY = 6.0f;
constexpr float kCornerRadius = 4.0f;

std::unordered_map<const Element*, std::wstring>& TooltipStore() {
    static std::unordered_map<const Element*, std::wstring> store;
    return store;
}

}  // namespace

Ref<ToolTip> ToolTip::Create(std::wstring text) noexcept {
    Ref<ToolTip> tip(new ToolTip());
    tip->text_ = text;
    tip->label_ = Label::Create(std::move(text));
    tip->label_->SetParent(tip.Get());
    return tip;
}

ToolTip::~ToolTip() = default;

Size ToolTip::MeasureOverride(Size) noexcept {
    if (!label_) {
        return Size{24.0f, 24.0f};
    }
    label_->Measure(Size{8192.0f, 8192.0f});
    const Size text = label_->DesiredSize();
    return Size{text.width + kPaddingX * 2.0f, text.height + kPaddingY * 2.0f};
}

void ToolTip::PaintContent(ID2D1RenderTarget* target) noexcept {
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
    if (label_) {
        const Size text = label_->DesiredSize();
        label_->Arrange(Rect{
            bounds_.x + kPaddingX,
            bounds_.y + kPaddingY,
            text.width,
            text.height});
        label_->Paint(target);
    }
}

void ToolTip::CollectChildren(std::vector<Element*>& elements) noexcept {
    if (label_) {
        elements.push_back(label_.Get());
    }
}

AccessibleRole ToolTip::AccessibilityRole() const noexcept {
    return AccessibleRole::Text;
}

std::wstring ToolTip::AccessibilityName() const {
    return text_;
}

void ToolTipService::SetToolTip(Element* owner, std::wstring text) {
    if (owner == nullptr) {
        return;
    }
    if (text.empty()) {
        TooltipStore().erase(owner);
    } else {
        TooltipStore().insert_or_assign(owner, std::move(text));
    }
}

std::wstring ToolTipService::GetToolTip(const Element* owner) {
    if (owner == nullptr) {
        return {};
    }
    auto found = TooltipStore().find(owner);
    return found == TooltipStore().end() ? std::wstring() : found->second;
}

void ToolTipService::ClearToolTip(const Element* owner) {
    TooltipStore().erase(owner);
}

}  // namespace nopeui
