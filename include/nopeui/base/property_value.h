#ifndef NOPEUI_BASE_PROPERTY_VALUE_H_
#define NOPEUI_BASE_PROPERTY_VALUE_H_

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "nopeui/base/geometry.h"

namespace nopeui {

enum class PropertyValueType {
    Bool,
    Int,
    Float,
    Double,
    Color,
    String,
    Thickness,
};

enum class PropertyFlags : std::uint32_t {
    None = 0,
    AffectsMeasure = 1u << 0,
    AffectsArrange = 1u << 1,
    AffectsRender = 1u << 2,
    Inherits = 1u << 3,
};

constexpr PropertyFlags operator|(PropertyFlags left, PropertyFlags right) noexcept {
    return static_cast<PropertyFlags>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr bool HasFlag(PropertyFlags value, PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
}

// Type-erased property payload used by the metadata table, style setters and
// change notifications. Colors are 0xRRGGBB, matching Theme.
class PropertyValue {
public:
    PropertyValue() = default;
    explicit PropertyValue(bool value) : value_(value) {}
    explicit PropertyValue(int value) : value_(value) {}
    explicit PropertyValue(float value) : value_(value) {}
    explicit PropertyValue(double value) : value_(value) {}
    explicit PropertyValue(std::uint32_t color) : value_(color) {}
    explicit PropertyValue(std::wstring value) : value_(std::move(value)) {}
    explicit PropertyValue(Thickness value) : value_(value) {}

    PropertyValueType Type() const noexcept {
        if (std::holds_alternative<bool>(value_)) {
            return PropertyValueType::Bool;
        }
        if (std::holds_alternative<int>(value_)) {
            return PropertyValueType::Int;
        }
        if (std::holds_alternative<float>(value_)) {
            return PropertyValueType::Float;
        }
        if (std::holds_alternative<double>(value_)) {
            return PropertyValueType::Double;
        }
        if (std::holds_alternative<std::uint32_t>(value_)) {
            return PropertyValueType::Color;
        }
        if (std::holds_alternative<Thickness>(value_)) {
            return PropertyValueType::Thickness;
        }
        return PropertyValueType::String;
    }

    bool AsBool(bool* out) const noexcept {
        if (!std::holds_alternative<bool>(value_)) {
            return false;
        }
        *out = std::get<bool>(value_);
        return true;
    }

    bool AsInt(int* out) const noexcept {
        if (!std::holds_alternative<int>(value_)) {
            return false;
        }
        *out = std::get<int>(value_);
        return true;
    }

    bool AsFloat(float* out) const noexcept {
        if (!std::holds_alternative<float>(value_)) {
            return false;
        }
        *out = std::get<float>(value_);
        return true;
    }

    bool AsDouble(double* out) const noexcept {
        if (!std::holds_alternative<double>(value_)) {
            return false;
        }
        *out = std::get<double>(value_);
        return true;
    }

    bool AsColor(std::uint32_t* out) const noexcept {
        if (!std::holds_alternative<std::uint32_t>(value_)) {
            return false;
        }
        *out = std::get<std::uint32_t>(value_);
        return true;
    }

    bool AsString(std::wstring* out) const {
        if (!std::holds_alternative<std::wstring>(value_)) {
            return false;
        }
        *out = std::get<std::wstring>(value_);
        return true;
    }

    bool AsThickness(Thickness* out) const noexcept {
        if (!std::holds_alternative<Thickness>(value_)) {
            return false;
        }
        *out = std::get<Thickness>(value_);
        return true;
    }

private:
    std::variant<bool, int, float, double, std::uint32_t, std::wstring, Thickness> value_;
};

struct PropertyMetadata {
    const wchar_t* name = L"";
    PropertyValueType type = PropertyValueType::String;
    PropertyFlags flags = PropertyFlags::None;
    PropertyValue default_value;
};

}  // namespace nopeui

#endif  // NOPEUI_BASE_PROPERTY_VALUE_H_
