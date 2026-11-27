#ifndef NOPEUI_THEME_CONTROL_TEMPLATE_H_
#define NOPEUI_THEME_CONTROL_TEMPLATE_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "nopeui/base/geometry.h"
#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// A named visual part a control reads while painting. This is the legacy
// template contract: controls own layout, the template owns the parts' colors
// and geometry hints.
struct TemplatePart {
    std::uint32_t background = 0;  // 0 means use the theme value
    std::uint32_t foreground = 0;
    std::uint32_t border = 0;
    float corner_radius = 0.0f;
    Thickness padding{};
};

// One live template binding: when source_property changes on the templated
// control, its current value is written to target_property on target.
struct TemplateBinding {
    Ref<Element> target;
    std::wstring target_property;
    std::wstring source_property;
};

struct TemplateInstance {
    Ref<Element> root;
    std::vector<TemplateBinding> bindings;
};

using TemplateFactory = std::function<TemplateInstance()>;

class ControlTemplate : public RefCounted {
public:
    static Ref<ControlTemplate> Create() noexcept;

    ControlTemplate& SetTargetType(std::wstring type) noexcept {
        target_type_ = std::move(type);
        return *this;
    }

    const std::wstring& TargetType() const noexcept {
        return target_type_;
    }

    ControlTemplate& SetPart(std::wstring name, TemplatePart part) noexcept {
        parts_.insert_or_assign(std::move(name), part);
        return *this;
    }

    const TemplatePart* Find(std::wstring_view name) const noexcept {
        auto found = parts_.find(std::wstring(name));
        return found == parts_.end() ? nullptr : &found->second;
    }

    // Visual-tree templates are instantiated per control. Controls that do not
    // consume a factory keep using the named parts above.
    ControlTemplate& SetFactory(TemplateFactory factory) noexcept {
        factory_ = std::move(factory);
        return *this;
    }

    bool HasFactory() const noexcept {
        return static_cast<bool>(factory_);
    }

    TemplateInstance Instantiate() const {
        return factory_ ? factory_() : TemplateInstance{};
    }

    // Visual states patch the named parts while a state is active. A patch only
    // overwrites the fields it actually sets: zero colors and a non-positive
    // corner radius keep the base part, and an all-zero padding does the same.
    ControlTemplate& SetStatePart(
        std::wstring group,
        std::wstring state,
        std::wstring part,
        TemplatePart patch) noexcept {
        states_[std::move(group)][std::move(state)][std::move(part)] = patch;
        return *this;
    }

    bool HasState(std::wstring_view group) const noexcept {
        return states_.find(std::wstring(group)) != states_.end();
    }

    // Base part merged with the patch of the given group/state, if any.
    TemplatePart ResolvePart(
        std::wstring_view part,
        std::wstring_view group,
        std::wstring_view state) const noexcept {
        TemplatePart resolved{};
        if (const TemplatePart* base = Find(part)) {
            resolved = *base;
        }
        const auto group_it = states_.find(std::wstring(group));
        if (group_it == states_.end()) {
            return resolved;
        }
        const auto state_it = group_it->second.find(std::wstring(state));
        if (state_it == group_it->second.end()) {
            return resolved;
        }
        const auto part_it = state_it->second.find(std::wstring(part));
        if (part_it == state_it->second.end()) {
            return resolved;
        }
        const TemplatePart& patch = part_it->second;
        if (patch.background != 0) {
            resolved.background = patch.background;
        }
        if (patch.foreground != 0) {
            resolved.foreground = patch.foreground;
        }
        if (patch.border != 0) {
            resolved.border = patch.border;
        }
        if (patch.corner_radius > 0.0f) {
            resolved.corner_radius = patch.corner_radius;
        }
        if (patch.padding.left != 0.0f || patch.padding.top != 0.0f ||
            patch.padding.right != 0.0f || patch.padding.bottom != 0.0f) {
            resolved.padding = patch.padding;
        }
        return resolved;
    }

private:
    ControlTemplate() noexcept = default;

    std::wstring target_type_;
    std::unordered_map<std::wstring, TemplatePart> parts_;
    // group -> state -> part -> patch
    std::unordered_map<
        std::wstring,
        std::unordered_map<
            std::wstring,
            std::unordered_map<std::wstring, TemplatePart>>>
        states_;
    TemplateFactory factory_;
};

}  // namespace nopeui

#endif  // NOPEUI_THEME_CONTROL_TEMPLATE_H_
