#ifndef NOPEUI_LAYOUT_PROPERTY_TRIGGER_H_
#define NOPEUI_LAYOUT_PROPERTY_TRIGGER_H_

#include <string>
#include <utility>
#include <vector>

#include "nopeui/base/property_value.h"

namespace nopeui {

// Watches one property of the element a style is applied to. While the value
// matches, every setter is written into the trigger layer, which sits above
// style values and below local values.
struct PropertyTrigger {
    std::wstring property;
    PropertyValue value;
    std::vector<std::pair<std::wstring, PropertyValue>> setters;
};

}  // namespace nopeui

#endif  // NOPEUI_LAYOUT_PROPERTY_TRIGGER_H_
