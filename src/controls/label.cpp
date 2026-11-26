#include "nopeui/controls/label.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "nopeui/theme/theme.h"

namespace nopeui {

Ref<Label> Label::Create(std::wstring text, graphics::TextStyle style) noexcept {
    Ref<Label> label(new Label());
    label->text_ = std::move(text);
    label->style_ = style;
    label->RebuildRun(8192.0f);
    return label;
}

void Label::SetTextStyle(graphics::TextStyle style) noexcept {
    if (style_ == style) {
        return;
    }
    style_ = style;
    run_dirty_ = true;
    InvalidateLayout();
}

void Label::SetColor(std::uint32_t color) noexcept {
    if (color_.Set(color)) {
        Invalidate();
    }
}

void Label::RebuildRun(float width) noexcept {
    run_width_ = width;
    run_dirty_ = false;
    auto created = graphics::TextRun::Create(text_, style_, width);
    if (created.ok()) {
        run_ = std::move(created.value());
    } else {
        run_.reset();
    }
}

Size Label::MeasureOverride(Size available) noexcept {
    const float width = available.width > 1.0f ? available.width : 1.0f;
    const bool width_matters =
        style_.wrap || style_.ellipsis ||
        style_.alignment != graphics::TextAlignment::Leading;
    if (run_dirty_ || (width_matters && std::abs(width - run_width_) > 1.0f)) {
        RebuildRun(width);
    }

    if (run_ != nullptr) {
        const Size measured = run_->Measure();
        const bool fills_width =
            style_.wrap || style_.alignment != graphics::TextAlignment::Leading;
        const float desired_width = fills_width
            ? available.width
            : std::min(measured.width, available.width);
        return Size{
            desired_width,
            std::min(measured.height, available.height)};
    }
    return Size{
        std::min(static_cast<float>(text_.size()) * 8.0f, available.width),
        20.0f};
}

void Label::PaintContent(ID2D1RenderTarget* target) noexcept {
    if (run_ == nullptr) {
        return;
    }
    const std::uint32_t color = color_.Get() != 0 ? color_.Get() : Colors().text;
    run_->Draw(target, Point{bounds_.x, bounds_.y}, color);
}

AccessibleRole Label::AccessibilityRole() const noexcept {
    return AccessibleRole::Text;
}

std::wstring Label::AccessibilityName() const {
    return text_;
}

}  // namespace nopeui
