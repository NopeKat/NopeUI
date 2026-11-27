#include "nopeui/controls/canvas.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>

#include <algorithm>
#include <iterator>

#include "nopeui/base/geometry.h"
#include "nopeui/layout/attached.h"

namespace nopeui {

namespace {

// Canvas measures children as if the axis had no limit, matching WPF.
constexpr float kUnbounded = 1.0e5f;

float ReadFloat(const Element& element, const AttachedPropertyMetadata& metadata) noexcept {
    const PropertyValue* stored = element.FindAttachedProperty(metadata);
    float value = 0.0f;
    if (stored != nullptr) {
        stored->AsFloat(&value);
    }
    return value;
}

}  // namespace

Ref<Canvas> Canvas::Create() noexcept {
    return Ref<Canvas>(new Canvas());
}

void Canvas::Add(Ref<Element> child) noexcept {
    if (!child) {
        return;
    }
    child->SetParent(this);
    children_.push_back(std::move(child));
    InvalidateLayout();
}

Size Canvas::MeasureOverride(Size available) noexcept {
    float width = 0.0f;
    float height = 0.0f;
    for (const auto& child : children_) {
        child->Measure(Size{kUnbounded, kUnbounded});
        const Size desired = child->DesiredSize();
        const float left = ReadFloat(*child, CanvasLeftProperty());
        const float top = ReadFloat(*child, CanvasTopProperty());
        width = std::max(width, left + desired.width);
        height = std::max(height, top + desired.height);
    }
    width = std::max(0.0f, width);
    height = std::max(0.0f, height);
    return Size{
        std::min(width, available.width >= kUnbounded ? width : available.width),
        std::min(height, available.height >= kUnbounded ? height : available.height)};
}

Size Canvas::ArrangeOverride(Size final_size) noexcept {
    for (const auto& child : children_) {
        const float left = ReadFloat(*child, CanvasLeftProperty());
        const float top = ReadFloat(*child, CanvasTopProperty());
        const Size desired = child->DesiredSize();
        // Children are stored in absolute coordinates, like every other panel,
        // so the canvas origin has to be added to the attached offsets.
        child->Arrange(Rect{
            bounds_.x + left,
            bounds_.y + top,
            desired.width,
            desired.height});
    }
    return final_size;
}

void Canvas::PaintContent(ID2D1RenderTarget* target) noexcept {
    PushContentClip(target);
    for (const auto& child : children_) {
        child->Paint(target);
    }
    PopContentClip(target);
}

Element* Canvas::HitTestOverride(Point point) noexcept {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Element* hit = (*it)->HitTest(point);
        if (hit != nullptr) {
            return hit;
        }
    }
    return nullptr;
}

void Canvas::CollectFocusable(std::vector<Element*>& elements) noexcept {
    if (!IsEnabled()) {
        return;
    }
    for (const auto& child : children_) {
        child->CollectFocusable(elements);
    }
}

void Canvas::CollectChildren(std::vector<Element*>& elements) noexcept {
    for (const auto& child : children_) {
        elements.push_back(child.Get());
    }
}

}  // namespace nopeui
