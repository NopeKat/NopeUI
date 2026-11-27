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

const AttachedPropertyMetadata* const kRegistry[] = {
    &kGridRow,
    &kGridColumn,
    &kGridRowSpan,
    &kGridColumnSpan,
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

}  // namespace nopeui
