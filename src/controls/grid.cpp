#include "nopeui/controls/grid.h"

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

// Anything past this counts as unbounded, matching what ScrollViewer offers.
constexpr float kUnbounded = 1.0e5f;

const PropertyMetadata kGridProperties[] = {
    {L"ColumnSpacing", PropertyValueType::Float, PropertyFlags::AffectsMeasure,
     PropertyValue(0.0f)},
    {L"RowSpacing", PropertyValueType::Float, PropertyFlags::AffectsMeasure,
     PropertyValue(0.0f)},
};

int TrackCount(const std::vector<GridLength>& definitions) noexcept {
    return definitions.empty() ? 1 : static_cast<int>(definitions.size());
}

int ClampIndex(int index, int count) noexcept {
    return std::clamp(index, 0, count - 1);
}

int ReadInt(const Element& element, const AttachedPropertyMetadata& metadata) noexcept {
    const PropertyValue* stored = element.FindAttachedProperty(metadata);
    int value = 0;
    if (stored != nullptr) {
        stored->AsInt(&value);
    }
    return value;
}

// Star tracks cannot divide an unbounded axis, so they fall back to content
// sizing there.
bool ContentSized(GridLength track, bool unbounded) noexcept {
    return track.unit == GridUnitType::Auto ||
           (unbounded && track.unit == GridUnitType::Star);
}

// Distributes the space that fixed and auto tracks leave over the star tracks.
void DistributeStars(
    const std::vector<GridLength>& definitions,
    int count,
    float available,
    float spacing,
    float* sizes) noexcept {
    float fixed = 0.0f;
    float weight = 0.0f;
    int star_count = 0;
    for (int index = 0; index < count; ++index) {
        const GridLength& track = definitions.empty()
            ? GridLength{1.0f, GridUnitType::Star}
            : definitions[static_cast<std::size_t>(index)];
        if (track.unit == GridUnitType::Star) {
            weight += track.Weight();
            ++star_count;
        } else {
            fixed += sizes[index];
        }
    }
    if (star_count == 0) {
        return;
    }
    const float gaps = spacing * static_cast<float>(count - 1);
    float remaining = available - fixed - gaps;
    const bool unbounded = available >= kUnbounded || remaining <= 0.0f;
    if (unbounded || weight <= 0.0f) {
        return;
    }
    float allocated = 0.0f;
    for (int index = 0; index < count; ++index) {
        const GridLength& track = definitions.empty()
            ? GridLength{1.0f, GridUnitType::Star}
            : definitions[static_cast<std::size_t>(index)];
        if (track.unit != GridUnitType::Star) {
            continue;
        }
        const float share = remaining * track.Weight() / weight;
        sizes[index] = share;
        allocated += share;
    }
    // Round the residual into the last star track so the total stays exact.
    for (int index = count - 1; index >= 0; --index) {
        const GridLength& track = definitions.empty()
            ? GridLength{1.0f, GridUnitType::Star}
            : definitions[static_cast<std::size_t>(index)];
        if (track.unit == GridUnitType::Star) {
            sizes[index] += remaining - allocated;
            break;
        }
    }
}

}  // namespace

GridCell ReadGridCell(const Element& element) noexcept {
    GridCell cell;
    cell.row = std::max(0, ReadInt(element, GridRowProperty()));
    cell.column = std::max(0, ReadInt(element, GridColumnProperty()));
    cell.row_span = std::max(1, ReadInt(element, GridRowSpanProperty()));
    cell.column_span = std::max(1, ReadInt(element, GridColumnSpanProperty()));
    return cell;
}

Ref<Grid> Grid::Create() noexcept {
    return Ref<Grid>(new Grid());
}

void Grid::Add(Ref<Element> child) noexcept {
    if (!child) {
        return;
    }
    child->SetParent(this);
    children_.push_back(std::move(child));
    InvalidateLayout();
}

void Grid::SetColumns(std::vector<GridLength> columns) noexcept {
    columns_ = std::move(columns);
    InvalidateLayout();
}

void Grid::SetRows(std::vector<GridLength> rows) noexcept {
    rows_ = std::move(rows);
    InvalidateLayout();
}

void Grid::AddColumn(GridLength column) noexcept {
    columns_.push_back(column);
    InvalidateLayout();
}

void Grid::AddRow(GridLength row) noexcept {
    rows_.push_back(row);
    InvalidateLayout();
}

void Grid::SetColumnSpacing(float spacing) noexcept {
    const float value = std::max(0.0f, spacing);
    if (column_spacing_ == value) {
        return;
    }
    column_spacing_ = value;
    InvalidateLayout();
}

void Grid::SetRowSpacing(float spacing) noexcept {
    const float value = std::max(0.0f, spacing);
    if (row_spacing_ == value) {
        return;
    }
    row_spacing_ = value;
    InvalidateLayout();
}

const PropertyMetadata* Grid::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kGridProperties);
    }
    return kGridProperties;
}

bool Grid::GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept {
    if (name == L"ColumnSpacing") {
        *value = PropertyValue(column_spacing_);
        return true;
    }
    if (name == L"RowSpacing") {
        *value = PropertyValue(row_spacing_);
        return true;
    }
    return false;
}

bool Grid::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    float spacing = 0.0f;
    if (!value.AsFloat(&spacing)) {
        return false;
    }
    if (name == L"ColumnSpacing") {
        SetColumnSpacing(spacing);
        return true;
    }
    if (name == L"RowSpacing") {
        SetRowSpacing(spacing);
        return true;
    }
    return false;
}

void Grid::ComputeColumns(Size available) noexcept {
    const int count = TrackCount(columns_);
    column_sizes_.assign(static_cast<std::size_t>(count), 0.0f);
    for (int index = 0; index < count; ++index) {
        if (!columns_.empty() &&
            columns_[static_cast<std::size_t>(index)].unit == GridUnitType::Pixel) {
            column_sizes_[static_cast<std::size_t>(index)] =
                std::max(0.0f, columns_[static_cast<std::size_t>(index)].value);
        }
    }

    // Natural pass: measure every child so auto columns can take the widest
    // content they hold. Children in an auto column are measured without a
    // width limit, otherwise a long label clamps to the row and pushes every
    // star column to zero.
    const bool unbounded = available.width >= kUnbounded;
    for (const auto& child : children_) {
        const GridCell cell = ReadGridCell(*child);
        const int start = ClampIndex(cell.column, count);
        const int span = std::clamp(cell.column_span, 1, count - start);
        bool auto_width = span == 1;
        if (auto_width) {
            const GridLength track = columns_.empty()
                ? GridLength{1.0f, GridUnitType::Star}
                : columns_[static_cast<std::size_t>(start)];
            auto_width = ContentSized(track, unbounded);
        }
        child->Measure(Size{
            auto_width ? kUnbounded : available.width,
            available.height});
        const Size desired = child->DesiredSize();
        // Spanning children do not drive an auto track: their natural size
        // already covers several cells, so letting them size one track would
        // blow the layout up. They are measured again once the tracks exist.
        if (span > 1) {
            continue;
        }
        int auto_tracks = 0;
        for (int offset = 0; offset < span; ++offset) {
            const GridLength track = columns_.empty()
                ? GridLength{1.0f, GridUnitType::Star}
                : columns_[static_cast<std::size_t>(start + offset)];
            if (ContentSized(track, unbounded)) {
                ++auto_tracks;
            }
        }
        if (auto_tracks == 0) {
            continue;
        }
        const float share = desired.width / static_cast<float>(auto_tracks);
        for (int offset = 0; offset < span; ++offset) {
            const std::size_t track_index = static_cast<std::size_t>(start + offset);
            const GridLength track = columns_.empty()
                ? GridLength{1.0f, GridUnitType::Star}
                : columns_[track_index];
            if (ContentSized(track, unbounded)) {
                column_sizes_[track_index] =
                    std::max(column_sizes_[track_index], share);
            }
        }
    }

    DistributeStars(columns_, count, available.width, column_spacing_, column_sizes_.data());
}

void Grid::ComputeRows(Size available) noexcept {
    const int count = TrackCount(rows_);
    row_sizes_.assign(static_cast<std::size_t>(count), 0.0f);
    for (int index = 0; index < count; ++index) {
        if (!rows_.empty() && rows_[static_cast<std::size_t>(index)].unit == GridUnitType::Pixel) {
            row_sizes_[static_cast<std::size_t>(index)] =
                std::max(0.0f, rows_[static_cast<std::size_t>(index)].value);
        }
    }

    // Height pass after the columns are known: the child is measured again at
    // the width of its span, so wrapped text reports the right line count.
    const bool unbounded = available.height >= kUnbounded;
    for (const auto& child : children_) {
        const GridCell cell = ReadGridCell(*child);
        const int column_count = TrackCount(columns_);
        const int column = ClampIndex(cell.column, column_count);
        const int column_span = std::clamp(cell.column_span, 1, column_count - column);
        float width = column_spacing_ * static_cast<float>(column_span - 1);
        for (int offset = 0; offset < column_span; ++offset) {
            width += column_sizes_[static_cast<std::size_t>(column + offset)];
        }
        child->Measure(Size{width, available.height});
        const Size desired = child->DesiredSize();
        const int row = ClampIndex(cell.row, count);
        const int span = std::clamp(cell.row_span, 1, count - row);
        if (span > 1) {
            continue;
        }
        int auto_tracks = 0;
        for (int offset = 0; offset < span; ++offset) {
            const GridLength track = rows_.empty()
                ? GridLength{1.0f, GridUnitType::Star}
                : rows_[static_cast<std::size_t>(row + offset)];
            if (ContentSized(track, unbounded)) {
                ++auto_tracks;
            }
        }
        if (auto_tracks == 0) {
            continue;
        }
        const float share = desired.height / static_cast<float>(auto_tracks);
        for (int offset = 0; offset < span; ++offset) {
            const std::size_t track_index = static_cast<std::size_t>(row + offset);
            const GridLength track = rows_.empty()
                ? GridLength{1.0f, GridUnitType::Star}
                : rows_[track_index];
            if (ContentSized(track, unbounded)) {
                row_sizes_[track_index] = std::max(row_sizes_[track_index], share);
            }
        }
    }

    DistributeStars(rows_, count, available.height, row_spacing_, row_sizes_.data());
}

Size Grid::MeasureOverride(Size available) noexcept {
    ComputeColumns(available);
    ComputeRows(available);

    float width = column_spacing_ * static_cast<float>(column_sizes_.size() - 1);
    for (const float size : column_sizes_) {
        width += size;
    }
    float height = row_spacing_ * static_cast<float>(row_sizes_.size() - 1);
    for (const float size : row_sizes_) {
        height += size;
    }
    // Auto rows can exceed the available height when content wraps. Report the
    // real height here; clipping it makes Measure and Arrange disagree and puts
    // the last row outside the parent's hit-test bounds.
    return Size{
        std::min(width, available.width >= kUnbounded ? width : available.width),
        height};
}

Size Grid::ArrangeOverride(Size final_size) noexcept {
    const int column_count = TrackCount(columns_);
    const int row_count = TrackCount(rows_);
    for (const auto& child : children_) {
        const GridCell cell = ReadGridCell(*child);
        const int column = ClampIndex(cell.column, column_count);
        const int row = ClampIndex(cell.row, row_count);
        const int column_span = std::clamp(cell.column_span, 1, column_count - column);
        const int row_span = std::clamp(cell.row_span, 1, row_count - row);

        float x = bounds_.x;
        for (int index = 0; index < column; ++index) {
            x += column_sizes_[static_cast<std::size_t>(index)] + column_spacing_;
        }
        float y = bounds_.y;
        for (int index = 0; index < row; ++index) {
            y += row_sizes_[static_cast<std::size_t>(index)] + row_spacing_;
        }
        float width = column_spacing_ * static_cast<float>(column_span - 1);
        for (int offset = 0; offset < column_span; ++offset) {
            width += column_sizes_[static_cast<std::size_t>(column + offset)];
        }
        float height = row_spacing_ * static_cast<float>(row_span - 1);
        for (int offset = 0; offset < row_span; ++offset) {
            height += row_sizes_[static_cast<std::size_t>(row + offset)];
        }
        child->Arrange(Rect{x, y, width, height});
    }
    return final_size;
}

void Grid::PaintContent(ID2D1RenderTarget* target) noexcept {
    PushContentClip(target);
    for (const auto& child : children_) {
        child->Paint(target);
    }
    PopContentClip(target);
}

Element* Grid::HitTestOverride(Point point) noexcept {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Element* hit = (*it)->HitTest(point);
        if (hit != nullptr) {
            return hit;
        }
    }
    return nullptr;
}

void Grid::CollectFocusable(std::vector<Element*>& elements) noexcept {
    if (!IsEnabled()) {
        return;
    }
    for (const auto& child : children_) {
        child->CollectFocusable(elements);
    }
}

void Grid::CollectChildren(std::vector<Element*>& elements) noexcept {
    for (const auto& child : children_) {
        elements.push_back(child.Get());
    }
}

}  // namespace nopeui
