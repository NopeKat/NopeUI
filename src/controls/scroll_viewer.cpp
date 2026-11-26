#include "nopeui/controls/scroll_viewer.h"

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

// Effectively unbounded height for measuring the scrollable content.
constexpr float kUnboundedHeight = 1.0e6f;
constexpr float kScrollbarWidth = 6.0f;
constexpr float kScrollbarMargin = 3.0f;
constexpr float kMinThumbHeight = 28.0f;
constexpr float kLineHeight = 16.0f;

bool ContainsPoint(const Rect& rect, Point point) noexcept {
    return point.x >= rect.x && point.x < rect.Right() &&
           point.y >= rect.y && point.y < rect.Bottom();
}

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

Rect ScrollViewer::TrackRect() const noexcept {
    if (content_height_ <= bounds_.height) {
        return Rect{};
    }
    return Rect{
        bounds_.Right() - kScrollbarMargin - kScrollbarWidth,
        bounds_.y + kScrollbarMargin,
        kScrollbarWidth,
        bounds_.height - kScrollbarMargin * 2.0f};
}

Rect ScrollViewer::ThumbRect() const noexcept {
    const Rect track = TrackRect();
    if (track.IsEmpty()) {
        return Rect{};
    }
    const float ratio = bounds_.height / content_height_;
    const float thumb_height = std::max(kMinThumbHeight, track.height * ratio);
    const float range = track.height - thumb_height;
    const float max_offset = MaxOffset();
    const float progress = max_offset > 0.0f ? offset_ / max_offset : 0.0f;
    return Rect{
        track.x,
        track.y + progress * range,
        kScrollbarWidth,
        thumb_height};
}

void ScrollViewer::PaintContent(ID2D1RenderTarget* target) noexcept {
    PushContentClip(target);
    if (child_) {
        child_->Paint(target);
    }
    PopContentClip(target);

    const Rect track = TrackRect();
    if (track.IsEmpty()) {
        return;
    }
    const Theme& colors = Colors();
    ComPtr<ID2D1SolidColorBrush> track_brush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.scrollbar_track), track_brush.GetAddressOf()))) {
        target->FillRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(track.x, track.y, track.Right(), track.Bottom()),
                kScrollbarWidth / 2.0f,
                kScrollbarWidth / 2.0f),
            track_brush.Get());
    }

    const Rect thumb = ThumbRect();
    ComPtr<ID2D1SolidColorBrush> thumb_brush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.scrollbar_thumb), thumb_brush.GetAddressOf()))) {
        target->FillRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(thumb.x, thumb.y, thumb.Right(), thumb.Bottom()),
                kScrollbarWidth / 2.0f,
                kScrollbarWidth / 2.0f),
            thumb_brush.Get());
    }
}

Element* ScrollViewer::HitTestOverride(Point point) noexcept {
    const Rect thumb = ThumbRect();
    if (!thumb.IsEmpty() && ContainsPoint(thumb, point)) {
        return this;
    }
    if (child_) {
        return child_->HitTest(point);
    }
    return nullptr;
}

void ScrollViewer::CollectFocusable(std::vector<Element*>& elements) noexcept {
    if (!IsEnabled()) {
        return;
    }
    if (child_) {
        child_->CollectFocusable(elements);
    }
}

bool ScrollViewer::OnWheel(int delta) noexcept {
    if (!IsEnabled() || content_height_ <= bounds_.height) {
        return false;
    }
    UINT lines = 3;
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    const float step = lines == 0
        ? (bounds_.height - kScrollbarMargin * 2.0f) * 0.9f
        : static_cast<float>(lines) * kLineHeight;
    const float delta_pixels = step * static_cast<float>(delta) / 120.0f;
    const float next = std::clamp(offset_ - delta_pixels, 0.0f, MaxOffset());
    if (next == offset_) {
        return false;
    }
    offset_ = next;
    ArrangeOverride(Size{bounds_.width, bounds_.height});
    Invalidate();
    return true;
}

void ScrollViewer::OnPointerDown(Point point) noexcept {
    if (!IsEnabled()) {
        return;
    }
    const Rect thumb = ThumbRect();
    if (thumb.IsEmpty() || !ContainsPoint(thumb, point)) {
        return;
    }
    dragging_ = true;
    drag_start_y_ = point.y;
    drag_start_offset_ = offset_;
}

void ScrollViewer::OnPointerMove(Point point) noexcept {
    if (!dragging_) {
        return;
    }
    const Rect track = TrackRect();
    const float thumb_height = ThumbRect().height;
    const float range = track.height - thumb_height;
    if (range <= 0.0f) {
        return;
    }
    const float max_offset = MaxOffset();
    const float delta = (point.y - drag_start_y_) / range * max_offset;
    SetVerticalOffset(drag_start_offset_ + delta);
}

void ScrollViewer::OnPointerUp(Point) noexcept {
    dragging_ = false;
}

void ScrollViewer::BringChildIntoView(const Element* descendant) noexcept {
    if (descendant == nullptr || content_height_ <= bounds_.height) {
        return;
    }
    const Rect target = descendant->Bounds();
    const float content_top = target.y - (bounds_.y - offset_);
    const float content_bottom = content_top + target.height;
    float next = offset_;
    if (content_top < 0.0f) {
        next -= content_top;
    } else if (content_bottom > bounds_.height) {
        next += content_bottom - bounds_.height;
    }
    next = std::clamp(next, 0.0f, MaxOffset());
    if (next == offset_) {
        return;
    }
    offset_ = next;
    ArrangeOverride(Size{bounds_.width, bounds_.height});
}

}  // namespace nopeui
