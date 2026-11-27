#ifndef NOPEUI_LAYOUT_ATTACHED_H_
#define NOPEUI_LAYOUT_ATTACHED_H_

#include <string_view>

#include "nopeui/base/property_value.h"

namespace nopeui {

// Property owned by a layout panel but stored on the child element. Names use
// the "Owner.Name" form, which is also the attribute spelling in XAML.
struct AttachedPropertyMetadata {
    const wchar_t* name = L"";
    PropertyValueType type = PropertyValueType::Int;
    PropertyValue default_value;
};

// Looks up "Grid.Row" and friends. Returns null for unknown names.
const AttachedPropertyMetadata* FindAttachedProperty(std::wstring_view name) noexcept;

bool IsAttachedPropertyName(std::wstring_view name) noexcept;

// Registered keys. Grid reads these while measuring and arranging its children.
const AttachedPropertyMetadata& GridRowProperty() noexcept;
const AttachedPropertyMetadata& GridColumnProperty() noexcept;
const AttachedPropertyMetadata& GridRowSpanProperty() noexcept;
const AttachedPropertyMetadata& GridColumnSpanProperty() noexcept;

// Canvas positions a child by these, DockPanel claims an edge by Dock.
const AttachedPropertyMetadata& CanvasLeftProperty() noexcept;
const AttachedPropertyMetadata& CanvasTopProperty() noexcept;
const AttachedPropertyMetadata& DockProperty() noexcept;

// FlexGrow is the share of leftover main-axis space a FlexPanel child asks for.
const AttachedPropertyMetadata& FlexGrowProperty() noexcept;

}  // namespace nopeui

#endif  // NOPEUI_LAYOUT_ATTACHED_H_
