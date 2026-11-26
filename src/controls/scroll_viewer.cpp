#include "nopeui/controls/scroll_viewer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <algorithm>
#include <utility>

namespace nopeui {
namespace {

// Effectively unbounded height for measuring the scrollable content.
constexpr float kUnboundedHeight = 1.0e6f;
constexpr float kLineStep = 48.0f;

}  // namespace

Ref<ScrollViewer> ScrollViewer::Create() noexcept {
    return Ref<ScrollViewer>(new ScrollViewer());
}

void ScrollViewer::SetChild(Ref<Element> child) noexcept {
    child_ = std::move(child);
    if (child_) {
        child_->SetParent(this);
    }
    offset_ = 0.0f;
    Invalidate();
}

float ScrollViewer::MaxOffset() const noexcept {
    return std::max(0.0f, content_height_ - bounds_.height);
}

void ScrollViewer::SetVerticalOffset(float offset) noexcept {
    const float clamped = std::clamp(offset, 0.0f, MaxOffset());
    if (clamped == offset_) {
        return;
    }
    offset_ = clamped;
    ArrangeOverride(Size{bounds_.width, bounds_.height});
    Invalidate();
}

Size ScrollViewer::MeasureOverride(Size available) noexcept {
    if (!child_) {
        content_height_ = 0.0f;
        return Size{available.width, available.height};
    }
    child_->Measure(Size{available.width, kUnboundedHeight});
    const Size content = child_->DesiredSize();
    content_height_ = content.height;
    return Size{
        std::min(content.width, available.width),
        std::min(content.height, available.height)};
}

Size ScrollViewer::ArrangeOverride(Size final_size) noexcept {
    offset_ = std::clamp(offset_, 0.0f, std::max(0.0f, content_height_ - final_size.height));
    if (child_) {
        const Size content = child_->DesiredSize();
        const float width = std::max(final_size.width, content.width);
        child_->Arrange(Rect{bounds_.x, bounds_.y - offset_, width, content.height});
    }
    return final_size;
}

void ScrollViewer::PaintContent(ID2D1RenderTarget* target) noexcept {
    PushContentClip(target);
    if (child_) {
        child_->Paint(target);
    }
    PopContentClip(target);
}

Element* ScrollViewer::HitTestOverride(Point point) noexcept {
    if (child_) {
        return child_->HitTest(point);
    }
    return nullptr;
}

void ScrollViewer::CollectFocusable(std::vector<Element*>& elements) noexcept {
    if (child_) {
        child_->CollectFocusable(elements);
    }
}

bool ScrollViewer::OnWheel(int delta) noexcept {
    if (content_height_ <= bounds_.height) {
        return false;
    }
    const float step = kLineStep * static_cast<float>(delta) / 120.0f;
    const float next = std::clamp(offset_ - step, 0.0f, MaxOffset());
    if (next == offset_) {
        return false;
    }
    offset_ = next;
    ArrangeOverride(Size{bounds_.width, bounds_.height});
    Invalidate();
    return true;
}

}  // namespace nopeui
