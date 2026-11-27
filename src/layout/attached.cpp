#include "nopeui/layout/attached.h"

#include <string>
#include <string_view>

namespace nopeui {

namespace {

const AttachedPropertyMetadata kGridRow{
    L"Grid.Row", PropertyValueType::Int, PropertyValue(0)};
const AttachedPropertyMetadata kGridColumn{
    L"Grid.Column", PropertyValueType::Int, PropertyValue(0)};
const AttachedPropertyMetadata kGridRowSpan{
    L"Grid.RowSpan", PropertyValueType::Int, PropertyValue(1)};
const AttachedPropertyMetadata kGridColumnSpan{
    L"Grid.ColumnSpan", PropertyValueType::Int, PropertyValue(1)};
const AttachedPropertyMetadata kCanvasLeft{
    L"Canvas.Left", PropertyValueType::Float, PropertyValue(0.0f)};
const AttachedPropertyMetadata kCanvasTop{
    L"Canvas.Top", PropertyValueType::Float, PropertyValue(0.0f)};
// Zero is Left, matching both the DockSide enum and the XAML default.
const AttachedPropertyMetadata kDock{
    L"Dock", PropertyValueType::Int, PropertyValue(0)};
const AttachedPropertyMetadata kFlexGrow{
    L"FlexGrow", PropertyValueType::Float, PropertyValue(0.0f)};

const AttachedPropertyMetadata* const kRegistry[] = {
    &kGridRow,
    &kGridColumn,
    &kGridRowSpan,
    &kGridColumnSpan,
    &kCanvasLeft,
    &kCanvasTop,
    &kDock,
    &kFlexGrow,
};

}  // namespace

const AttachedPropertyMetadata* FindAttachedProperty(std::wstring_view name) noexcept {
    for (const AttachedPropertyMetadata* metadata : kRegistry) {
        if (std::wstring_view(metadata->name) == name) {
            return metadata;
        }
    }
    return nullptr;
}

bool IsAttachedPropertyName(std::wstring_view name) noexcept {
    return FindAttachedProperty(name) != nullptr;
}

const AttachedPropertyMetadata& GridRowProperty() noexcept {
    return kGridRow;
}

const AttachedPropertyMetadata& GridColumnProperty() noexcept {
    return kGridColumn;
}

const AttachedPropertyMetadata& GridRowSpanProperty() noexcept {
    return kGridRowSpan;
}

const AttachedPropertyMetadata& GridColumnSpanProperty() noexcept {
    return kGridColumnSpan;
}

const AttachedPropertyMetadata& CanvasLeftProperty() noexcept {
    return kCanvasLeft;
}

const AttachedPropertyMetadata& CanvasTopProperty() noexcept {
    return kCanvasTop;
}

const AttachedPropertyMetadata& DockProperty() noexcept {
    return kDock;
}

const AttachedPropertyMetadata& FlexGrowProperty() noexcept {
    return kFlexGrow;
}

}  // namespace nopeui
