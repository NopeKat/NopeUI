#include "nopeui/layout/style.h"

#include <utility>

#include "nopeui/layout/element.h"

namespace nopeui {

Ref<ControlTemplate> ControlTemplate::Create() noexcept {
    return Ref<ControlTemplate>(new ControlTemplate());
}

Ref<Style> Style::Create() noexcept {
    return Ref<Style>(new Style());
}

Style& Style::SetTargetType(std::wstring type) noexcept {
    target_type_ = std::move(type);
    return *this;
}

Style& Style::SetTargetKey(std::wstring key) noexcept {
    target_key_ = std::move(key);
    return *this;
}

Style& Style::Set(std::wstring name, PropertyValue value) noexcept {
    setters_.insert_or_assign(std::move(name), std::move(value));
    return *this;
}

Style& Style::SetTemplate(Ref<ControlTemplate> control_template) noexcept {
    control_template_ = std::move(control_template);
    return *this;
}

Style& Style::AddTrigger(PropertyTrigger trigger) noexcept {
    triggers_.push_back(std::move(trigger));
    return *this;
}

bool Style::Matches(const Element& element) const noexcept {
    if (!target_type_.empty() && target_type_ != element.TypeName()) {
        return false;
    }
    return target_key_.empty() || target_key_ == element.Key();
}

void Style::Apply(Element& element) const noexcept {
    if (!Matches(element)) {
        return;
    }
    for (const auto& setter : setters_) {
        element.ApplyStyleProperty(setter.first, setter.second);
    }
    if (control_template_) {
        element.ApplyControlTemplate(*control_template_);
    }
    for (const PropertyTrigger& trigger : triggers_) {
        element.AddTrigger(this, trigger);
    }
    if (!triggers_.empty()) {
        element.RefreshTriggers();
    }
}

Ref<StyleSheet> StyleSheet::Create() noexcept {
    return Ref<StyleSheet>(new StyleSheet());
}

void StyleSheet::Add(Ref<Style> style) noexcept {
    if (style) {
        styles_.push_back(std::move(style));
    }
}

void StyleSheet::ApplyTo(Element& root) const noexcept {
    for (const auto& style : styles_) {
        style->Apply(root);
    }
    std::vector<Element*> children;
    root.CollectChildren(children);
    for (Element* child : children) {
        if (child != nullptr) {
            ApplyTo(*child);
        }
    }
}

}  // namespace nopeui
