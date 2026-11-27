#include "nopeui/controls/flex_panel.h"

#include <algorithm>

#include "nopeui/base/geometry.h"
#include "nopeui/layout/attached.h"

namespace nopeui {

namespace {

const PropertyMetadata kFlexPanelProperties[] = {
    {L"Gap", PropertyValueType::Float, PropertyFlags::AffectsMeasure, PropertyValue(8.0f)},
    {L"Direction", PropertyValueType::Int, PropertyFlags::AffectsMeasure, PropertyValue(0)},
    {L"Wrap", PropertyValueType::Bool, PropertyFlags::AffectsMeasure, PropertyValue(false)},
};

}  // namespace

Ref<FlexPanel> FlexPanel::Create(float gap) noexcept {
    Ref<FlexPanel> panel(new FlexPanel());
    panel->gap_ = std::max(0.0f, gap);
    return panel;
}

void FlexPanel::Add(Ref<Element> child) noexcept {
    if (!child) {
        return;
    }
    child->SetParent(this);
    children_.push_back(std::move(child));
}

void FlexPanel::SetDirection(FlexDirection direction) noexcept {
    if (direction_ == direction) {
        return;
    }
    direction_ = direction;
    InvalidateLayout();
}

void FlexPanel::SetGap(float gap) noexcept {
    const float value = std::max(0.0f, gap);
    if (gap_ == value) {
        return;
    }
    gap_ = value;
    InvalidateLayout();
}

void FlexPanel::SetWrap(bool wrap) noexcept {
    if (wrap_ == wrap) {
        return;
    }
    wrap_ = wrap;
    InvalidateLayout();
}

const PropertyMetadata* FlexPanel::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kFlexPanelProperties);
    }
    return kFlexPanelProperties;
}

bool FlexPanel::GetControlProperty(
    std::wstring_view name,
    PropertyValue* value) const noexcept {
    if (name == L"Gap") {
        *value = PropertyValue(gap_);
        return true;
    }
    if (name == L"Direction") {
        *value = PropertyValue(IsRow() ? 0 : 1);
        return true;
    }
    if (name == L"Wrap") {
        *value = PropertyValue(wrap_);
        return true;
    }
    return false;
}

bool FlexPanel::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    if (name == L"Gap") {
        float gap = 0.0f;
        if (!value.AsFloat(&gap)) {
            return false;
        }
        SetGap(gap);
        return true;
    }
    if (name == L"Direction") {
        int direction = 0;
        if (!value.AsInt(&direction)) {
            return false;
        }
        if (direction == 0) {
            SetDirection(FlexDirection::Row);
        } else if (direction == 1) {
            SetDirection(FlexDirection::Column);
        } else {
            return false;
        }
        return true;
    }
    if (name == L"Wrap") {
        bool wrap = false;
        if (!value.AsBool(&wrap)) {
            return false;
        }
        SetWrap(wrap);
        return true;
    }
    return false;
}

float FlexPanel::GrowOf(const Element& child) const noexcept {
    PropertyValue value;
    float grow = 0.0f;
    if (!child.GetAttachedProperty(FlexGrowProperty().name, &value) ||
        !value.AsFloat(&grow)) {
        return 0.0f;
    }
    return std::max(0.0f, grow);
}

std::vector<FlexPanel::Line> FlexPanel::BuildLines(float main_limit) noexcept {
    std::vector<Line> lines;
    const bool row = IsRow();
    for (const auto& child : children_) {
        const Size measured = child->DesiredSize();
        const float main = row ? measured.width : measured.height;
        const float cross = row ? measured.height : measured.width;
        if (lines.empty()) {
            lines.emplace_back();
        }
        Line& line = lines.back();
        const float grown_main = line.main + (line.items.empty() ? 0.0f : gap_) + main;
        if (wrap_ && !line.items.empty() && main_limit > 0.0f &&
            grown_main > main_limit + 0.01f) {
            lines.emplace_back();
            Line& next = lines.back();
            next.items.push_back(child.Get());
            next.main = main;
            next.cross = cross;
            continue;
        }
        line.items.push_back(child.Get());
        line.main = grown_main;
        line.cross = std::max(line.cross, cross);
    }
    return lines;
}

Size FlexPanel::MeasureOverride(Size available) noexcept {
    for (const auto& child : children_) {
        child->Measure(available);
    }
    std::vector<Line> lines = BuildLines(IsRow() ? available.width : available.height);
    float main_extent = 0.0f;
    float cross_extent = 0.0f;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        main_extent = std::max(main_extent, lines[index].main);
        cross_extent += lines[index].cross;
        if (index + 1 < lines.size()) {
            cross_extent += gap_;
        }
    }
    const float cross_limit = IsRow() ? available.height : available.width;
    const float clamped_cross = cross_limit > 0.0f
        ? std::min(cross_extent, cross_limit)
        : cross_extent;
    return IsRow() ? Size{main_extent, clamped_cross} : Size{clamped_cross, main_extent};
}

Size FlexPanel::ArrangeOverride(Size final_size) noexcept {
    const bool row = IsRow();
    const float main_limit = row ? final_size.width : final_size.height;
    std::vector<Line> lines = BuildLines(main_limit);
    const float full_cross = row ? final_size.height : final_size.width;
    float cross_offset = row ? bounds_.y : bounds_.x;
    for (const Line& line : lines) {
        // A single line stretches across the whole panel, like StackPanel; with
        // Wrap on each line keeps its own cross extent so lines stack.
        const float slot_cross = wrap_ ? line.cross : std::max(line.cross, full_cross);
        float total_grow = 0.0f;
        for (const Element* item : line.items) {
            total_grow += GrowOf(*item);
        }
        const float leftover = std::max(0.0f, main_limit - line.main);
        const float share = total_grow > 0.0f ? leftover / total_grow : 0.0f;
        float main_offset = row ? bounds_.x : bounds_.y;
        for (Element* item : line.items) {
            const Size measured = item->DesiredSize();
            const float main_size = (row ? measured.width : measured.height) +
                share * GrowOf(*item);
            if (row) {
                item->Arrange(Rect{main_offset, cross_offset, main_size, slot_cross});
                main_offset += main_size + gap_;
            } else {
                item->Arrange(Rect{cross_offset, main_offset, slot_cross, main_size});
                main_offset += main_size + gap_;
            }
        }
        cross_offset += line.cross + gap_;
    }
    return final_size;
}

void FlexPanel::PaintContent(ID2D1RenderTarget* target) noexcept {
    PushContentClip(target);
    for (const auto& child : children_) {
        child->Paint(target);
    }
    PopContentClip(target);
}

Element* FlexPanel::HitTestOverride(Point point) noexcept {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Element* hit = (*it)->HitTest(point);
        if (hit != nullptr) {
            return hit;
        }
    }
    return nullptr;
}

void FlexPanel::CollectFocusable(std::vector<Element*>& elements) noexcept {
    if (!IsEnabled()) {
        return;
    }
    for (const auto& child : children_) {
        child->CollectFocusable(elements);
    }
}

void FlexPanel::CollectChildren(std::vector<Element*>& elements) noexcept {
    for (const auto& child : children_) {
        elements.push_back(child.Get());
    }
}

}  // namespace nopeui
