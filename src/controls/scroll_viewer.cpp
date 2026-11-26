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
constexpr float kScrollbarHoverWidth = 10.0f;
constexpr float kScrollbarHitBand = 14.0f;
constexpr float kScrollbarMargin = 3.0f;
constexpr float kMinThumbSize = 28.0f;
constexpr float kLineHeight = 16.0f;

bool ContainsPoint(const Rect& rect, Point point) noexcept {
    return point.x >= rect.x && point.x < rect.Right() &&
           point.y >= rect.y && point.y < rect.Bottom();
}

float ThumbSize(float track, float content, float viewport) noexcept {
    if (content <= viewport || track <= 0.0f) {
        return 0.0f;
    }
    return std::max(kMinThumbSize, track * viewport / content);
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
    offset_x_ = 0.0f;
    offset_y_ = 0.0f;
    Invalidate();
}

float ScrollViewer::MaxVerticalOffset() const noexcept {
    return std::max(0.0f, content_height_ - bounds_.height);
}

float ScrollViewer::MaxHorizontalOffset() const noexcept {
    return std::max(0.0f, content_width_ - bounds_.width);
}

bool ScrollViewer::HasVerticalScrollbar() const noexcept {
    return content_height_ > bounds_.height + 0.5f;
}

bool ScrollViewer::HasHorizontalScrollbar() const noexcept {
    return content_width_ > bounds_.width + 0.5f;
}

float ScrollViewer::ScrollbarThickness() const noexcept {
    return (hovered_ || drag_axis_ != DragAxis::None)
        ? kScrollbarHoverWidth
        : kScrollbarWidth;
}

void ScrollViewer::SetVerticalOffset(float offset) noexcept {
    const float clamped = std::clamp(offset, 0.0f, MaxVerticalOffset());
    if (clamped == offset_y_) {
        return;
    }
    offset_y_ = clamped;
    ArrangeOverride(Size{bounds_.width, bounds_.height});
    Invalidate();
}

void ScrollViewer::SetHorizontalOffset(float offset) noexcept {
    const float clamped = std::clamp(offset, 0.0f, MaxHorizontalOffset());
    if (clamped == offset_x_) {
        return;
    }
    offset_x_ = clamped;
    ArrangeOverride(Size{bounds_.width, bounds_.height});
    Invalidate();
}

Size ScrollViewer::MeasureOverride(Size available) noexcept {
    if (!child_) {
        content_width_ = 0.0f;
        content_height_ = 0.0f;
        return Size{available.width, available.height};
    }
    child_->Measure(Size{available.width, kUnboundedHeight});
    const Size content = child_->DesiredSize();
    content_width_ = content.width;
    content_height_ = content.height;
    return Size{
        std::min(content.width, available.width),
        std::min(content.height, available.height)};
}

Size ScrollViewer::ArrangeOverride(Size final_size) noexcept {
    offset_x_ = std::clamp(offset_x_, 0.0f, std::max(0.0f, content_width_ - final_size.width));
    offset_y_ = std::clamp(offset_y_, 0.0f, std::max(0.0f, content_height_ - final_size.height));
    if (child_) {
        const Size content = child_->DesiredSize();
        const float width = std::max(final_size.width, content.width);
        const float height = std::max(final_size.height, content.height);
        child_->Arrange(Rect{
            bounds_.x - offset_x_,
            bounds_.y - offset_y_,
            width,
            height});
    }
    return final_size;
}

Rect ScrollViewer::VerticalTrackRect() const noexcept {
    if (!HasVerticalScrollbar()) {
        return Rect{};
    }
    const float thickness = ScrollbarThickness();
    const float right_edge = bounds_.Right() - kScrollbarMargin;
    const float top = bounds_.y + kScrollbarMargin;
    const float bottom = bounds_.Bottom() - kScrollbarMargin -
        (HasHorizontalScrollbar() ? thickness + kScrollbarMargin : 0.0f);
    if (bottom <= top) {
        return Rect{};
    }
    return Rect{right_edge - thickness, top, thickness, bottom - top};
}

Rect ScrollViewer::VerticalThumbRect() const noexcept {
    const Rect track = VerticalTrackRect();
    if (track.IsEmpty()) {
        return Rect{};
    }
    const float thumb_height = ThumbSize(track.height, content_height_, bounds_.height);
    if (thumb_height <= 0.0f) {
        return Rect{};
    }
    const float range = track.height - thumb_height;
    const float max_offset = MaxVerticalOffset();
    const float progress = max_offset > 0.0f ? offset_y_ / max_offset : 0.0f;
    return Rect{
        track.x,
        track.y + progress * range,
        track.width,
        thumb_height};
}

Rect ScrollViewer::HorizontalTrackRect() const noexcept {
    if (!HasHorizontalScrollbar()) {
        return Rect{};
    }
    const float thickness = ScrollbarThickness();
    const float left = bounds_.x + kScrollbarMargin;
    const float right = bounds_.Right() - kScrollbarMargin -
        (HasVerticalScrollbar() ? thickness + kScrollbarMargin : 0.0f);
    const float bottom_edge = bounds_.Bottom() - kScrollbarMargin;
    if (right <= left) {
        return Rect{};
    }
    return Rect{left, bottom_edge - thickness, right - left, thickness};
}

Rect ScrollViewer::HorizontalThumbRect() const noexcept {
    const Rect track = HorizontalTrackRect();
    if (track.IsEmpty()) {
        return Rect{};
    }
    const float thumb_width = ThumbSize(track.width, content_width_, bounds_.width);
    if (thumb_width <= 0.0f) {
        return Rect{};
    }
    const float range = track.width - thumb_width;
    const float max_offset = MaxHorizontalOffset();
    const float progress = max_offset > 0.0f ? offset_x_ / max_offset : 0.0f;
    return Rect{
        track.x + progress * range,
        track.y,
        thumb_width,
        track.height};
}

void ScrollViewer::PaintContent(ID2D1RenderTarget* target) noexcept {
    PushContentClip(target);
    if (child_) {
        child_->Paint(target);
    }
    PopContentClip(target);

    const Theme& colors = Colors();
    const float radius = ScrollbarThickness() / 2.0f;

    const Rect vertical_track = VerticalTrackRect();
    const Rect vertical_thumb = VerticalThumbRect();
    const Rect horizontal_track = HorizontalTrackRect();
    const Rect horizontal_thumb = HorizontalThumbRect();

    ComPtr<ID2D1SolidColorBrush> track_brush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.scrollbar_track), track_brush.GetAddressOf()))) {
        if (!vertical_track.IsEmpty()) {
            target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(
                        vertical_track.x,
                        vertical_track.y,
                        vertical_track.Right(),
                        vertical_track.Bottom()),
                    radius,
                    radius),
                track_brush.Get());
        }
        if (!horizontal_track.IsEmpty()) {
            target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(
                        horizontal_track.x,
                        horizontal_track.y,
                        horizontal_track.Right(),
                        horizontal_track.Bottom()),
                    radius,
                    radius),
                track_brush.Get());
        }
    }

    ComPtr<ID2D1SolidColorBrush> thumb_brush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(colors.scrollbar_thumb), thumb_brush.GetAddressOf()))) {
        if (!vertical_thumb.IsEmpty()) {
            target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(
                        vertical_thumb.x,
                        vertical_thumb.y,
                        vertical_thumb.Right(),
                        vertical_thumb.Bottom()),
                    radius,
                    radius),
                thumb_brush.Get());
        }
        if (!horizontal_thumb.IsEmpty()) {
            target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(
                        horizontal_thumb.x,
                        horizontal_thumb.y,
                        horizontal_thumb.Right(),
                        horizontal_thumb.Bottom()),
                    radius,
                    radius),
                thumb_brush.Get());
        }
    }
}

Element* ScrollViewer::HitTestOverride(Point point) noexcept {
    const float band = kScrollbarHitBand;
    if (HasVerticalScrollbar() &&
        point.x >= bounds_.Right() - kScrollbarMargin - band) {
        return this;
    }
    if (HasHorizontalScrollbar() &&
        point.y >= bounds_.Bottom() - kScrollbarMargin - band) {
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

void ScrollViewer::CollectChildren(std::vector<Element*>& elements) noexcept {
    if (child_) {
        elements.push_back(child_.Get());
    }
}

bool ScrollViewer::OnWheel(int delta) noexcept {
    if (!IsEnabled()) {
        return false;
    }
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool horizontal = HasHorizontalScrollbar() &&
        (!HasVerticalScrollbar() || shift);
    if (horizontal) {
        UINT lines = 3;
        SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
        const float step = lines == 0
            ? (bounds_.width - kScrollbarMargin * 2.0f) * 0.9f
            : static_cast<float>(lines) * kLineHeight;
        const float delta_pixels = step * static_cast<float>(delta) / 120.0f;
        const float next = std::clamp(
            offset_x_ - delta_pixels, 0.0f, MaxHorizontalOffset());
        if (next == offset_x_) {
            return false;
        }
        offset_x_ = next;
        ArrangeOverride(Size{bounds_.width, bounds_.height});
        Invalidate();
        return true;
    }
    if (!HasVerticalScrollbar()) {
        return false;
    }
    UINT lines = 3;
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    const float step = lines == 0
        ? (bounds_.height - kScrollbarMargin * 2.0f) * 0.9f
        : static_cast<float>(lines) * kLineHeight;
    const float delta_pixels = step * static_cast<float>(delta) / 120.0f;
    const float next = std::clamp(
        offset_y_ - delta_pixels, 0.0f, MaxVerticalOffset());
    if (next == offset_y_) {
        return false;
    }
    offset_y_ = next;
    ArrangeOverride(Size{bounds_.width, bounds_.height});
    Invalidate();
    return true;
}

bool ScrollViewer::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled() || !HasVerticalScrollbar()) {
        return false;
    }
    const float page = std::max(24.0f, bounds_.height * 0.9f);
    switch (key) {
        case VK_PRIOR:
            SetVerticalOffset(offset_y_ - page);
            return true;
        case VK_NEXT:
            SetVerticalOffset(offset_y_ + page);
            return true;
        case VK_HOME:
            SetVerticalOffset(0.0f);
            return true;
        case VK_END:
            SetVerticalOffset(MaxVerticalOffset());
            return true;
        default:
            return false;
    }
}

void ScrollViewer::OnPointerEnter() noexcept {
    if (!hovered_) {
        hovered_ = true;
        Invalidate();
    }
}

void ScrollViewer::OnPointerLeave() noexcept {
    if (drag_axis_ == DragAxis::None && hovered_) {
        hovered_ = false;
        Invalidate();
    }
}

void ScrollViewer::OnPointerDown(Point point) noexcept {
    if (!IsEnabled()) {
        return;
    }
    drag_axis_ = DragAxis::None;
    hovered_ = true;
    const Rect vertical = VerticalThumbRect();
    if (!vertical.IsEmpty() && ContainsPoint(vertical, point)) {
        drag_axis_ = DragAxis::Vertical;
        drag_start_y_ = point.y;
        drag_start_offset_y_ = offset_y_;
        Invalidate();
        return;
    }
    const Rect horizontal = HorizontalThumbRect();
    if (!horizontal.IsEmpty() && ContainsPoint(horizontal, point)) {
        drag_axis_ = DragAxis::Horizontal;
        drag_start_x_ = point.x;
        drag_start_offset_x_ = offset_x_;
    }
    Invalidate();
}

void ScrollViewer::OnPointerMove(Point point) noexcept {
    if (drag_axis_ == DragAxis::Vertical) {
        const Rect track = VerticalTrackRect();
        const float range = track.height - VerticalThumbRect().height;
        if (range <= 0.0f) {
            return;
        }
        const float delta =
            (point.y - drag_start_y_) / range * MaxVerticalOffset();
        SetVerticalOffset(drag_start_offset_y_ + delta);
    } else if (drag_axis_ == DragAxis::Horizontal) {
        const Rect track = HorizontalTrackRect();
        const float range = track.width - HorizontalThumbRect().width;
        if (range <= 0.0f) {
            return;
        }
        const float delta =
            (point.x - drag_start_x_) / range * MaxHorizontalOffset();
        SetHorizontalOffset(drag_start_offset_x_ + delta);
    }
}

void ScrollViewer::OnPointerUp(Point) noexcept {
    drag_axis_ = DragAxis::None;
    Invalidate();
}

void ScrollViewer::BringChildIntoView(const Element* descendant) noexcept {
    if (descendant == nullptr) {
        return;
    }
    const Rect target = descendant->Bounds();
    float next_x = offset_x_;
    float next_y = offset_y_;

    const float content_top = target.y - (bounds_.y - offset_y_);
    const float content_bottom = content_top + target.height;
    if (content_top < 0.0f) {
        next_y -= content_top;
    } else if (content_bottom > bounds_.height) {
        next_y += content_bottom - bounds_.height;
    }

    const float content_left = target.x - (bounds_.x - offset_x_);
    const float content_right = content_left + target.width;
    if (content_left < 0.0f) {
        next_x -= content_left;
    } else if (content_right > bounds_.width) {
        next_x += content_right - bounds_.width;
    }

    next_x = std::clamp(next_x, 0.0f, MaxHorizontalOffset());
    next_y = std::clamp(next_y, 0.0f, MaxVerticalOffset());
    if (next_x == offset_x_ && next_y == offset_y_) {
        return;
    }
    offset_x_ = next_x;
    offset_y_ = next_y;
    ArrangeOverride(Size{bounds_.width, bounds_.height});
}

}  // namespace nopeui
