#include "nopeui/controls/label.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <algorithm>
#include <utility>

#include "nopeui/graphics/text.h"

namespace nopeui {

Ref<Label> Label::Create(std::wstring text) noexcept {
    Ref<Label> label(new Label());
    label->text_ = std::move(text);
    auto run = graphics::TextRun::Create(label->text_, 14.0f, false);
    if (run.ok()) {
        label->run_ = std::move(run.value());
    }
    return label;
}

Size Label::MeasureOverride(Size available) noexcept {
    if (run_ != nullptr) {
        const Size measured = run_->Measure();
        return Size{
            std::min(measured.width, available.width),
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
    run_->Draw(target, Point{bounds_.x, bounds_.y}, 0x1B1B1B);
}

}  // namespace nopeui
