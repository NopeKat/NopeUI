#ifndef NOPEUI_CONTROLS_GRID_H_
#define NOPEUI_CONTROLS_GRID_H_

#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"
#include "nopeui/layout/layout_types.h"

namespace nopeui {

// Row/column container. Tracks are fixed pixels, Auto (content driven) or Star
// (share of what is left). Children pick their cell through the attached
// Grid.Row, Grid.Column, Grid.RowSpan and Grid.ColumnSpan properties.
//
// Measurement runs in three passes: columns from the natural size, rows from the
// size the children take once the columns are known, then arrangement. Star
// tracks collapse to content sizing when the axis is unbounded, which is what a
// ScrollViewer offers.
class Grid : public Element {
public:
    static Ref<Grid> Create() noexcept;

    void Add(Ref<Element> child) noexcept;

    void SetColumns(std::vector<GridLength> columns) noexcept;
    void SetRows(std::vector<GridLength> rows) noexcept;
    void AddColumn(GridLength column) noexcept;
    void AddRow(GridLength row) noexcept;

    const std::vector<GridLength>& Columns() const noexcept {
        return columns_;
    }
    const std::vector<GridLength>& Rows() const noexcept {
        return rows_;
    }

    void SetColumnSpacing(float spacing) noexcept;
    void SetRowSpacing(float spacing) noexcept;
    float ColumnSpacing() const noexcept {
        return column_spacing_;
    }
    float RowSpacing() const noexcept {
        return row_spacing_;
    }

    std::wstring_view TypeName() const noexcept override {
        return L"Grid";
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    Element* HitTestOverride(Point point) noexcept override;
    void CollectFocusable(std::vector<Element*>& elements) noexcept override;
    void CollectChildren(std::vector<Element*>& elements) noexcept override;
    const PropertyMetadata* ControlPropertyTable(std::size_t* count) const noexcept override;
    bool GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept override;
    bool SetControlProperty(
        std::wstring_view name,
        const PropertyValue& value) noexcept override;

private:
    Grid() noexcept = default;

    void ComputeColumns(Size available) noexcept;
    void ComputeRows(Size available) noexcept;

    std::vector<Ref<Element>> children_;
    std::vector<GridLength> columns_;
    std::vector<GridLength> rows_;
    std::vector<float> column_sizes_;
    std::vector<float> row_sizes_;
    float column_spacing_ = 0.0f;
    float row_spacing_ = 0.0f;
};

// Cell of a child, resolved against the current track counts.
struct GridCell {
    int row = 0;
    int column = 0;
    int row_span = 1;
    int column_span = 1;
};

GridCell ReadGridCell(const Element& element) noexcept;

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_GRID_H_
