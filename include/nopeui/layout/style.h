#ifndef NOPEUI_LAYOUT_STYLE_H_
#define NOPEUI_LAYOUT_STYLE_H_

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "nopeui/base/property_value.h"
#include "nopeui/base/ref.h"
#include "nopeui/theme/control_template.h"

namespace nopeui {

class Element;

// A style matches by element type name and, optionally, by key. It writes
// metadata-backed properties through Element::SetProperty and may attach a
// control template. Later styles in a sheet win over earlier ones.
class Style : public RefCounted {
public:
    static Ref<Style> Create() noexcept;

    Style& SetTargetType(std::wstring type) noexcept;
    Style& SetTargetKey(std::wstring key) noexcept;
    Style& Set(std::wstring name, PropertyValue value) noexcept;
    Style& SetTemplate(Ref<ControlTemplate> control_template) noexcept;

    bool Matches(const Element& element) const noexcept;
    void Apply(Element& element) const noexcept;

private:
    Style() noexcept = default;

    std::wstring target_type_;
    std::wstring target_key_;
    std::unordered_map<std::wstring, PropertyValue> setters_;
    Ref<ControlTemplate> control_template_;
};

class StyleSheet : public RefCounted {
public:
    static Ref<StyleSheet> Create() noexcept;

    void Add(Ref<Style> style) noexcept;

    // Applies matching styles to the element and all descendants.
    void ApplyTo(Element& root) const noexcept;

    const std::vector<Ref<Style>>& Styles() const noexcept {
        return styles_;
    }

private:
    StyleSheet() noexcept = default;

    std::vector<Ref<Style>> styles_;
};

}  // namespace nopeui

#endif  // NOPEUI_LAYOUT_STYLE_H_
