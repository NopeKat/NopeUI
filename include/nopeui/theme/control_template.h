#ifndef NOPEUI_THEME_CONTROL_TEMPLATE_H_
#define NOPEUI_THEME_CONTROL_TEMPLATE_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "nopeui/base/geometry.h"
#include "nopeui/base/ref.h"

namespace nopeui {

// A named visual part a control reads while painting. This is the minimal
// template contract: controls own layout, the template owns the parts' colors
// and geometry hints. Full visual-tree templating can grow from here.
struct TemplatePart {
    std::uint32_t background = 0;  // 0 means use the theme value
    std::uint32_t foreground = 0;
    std::uint32_t border = 0;
    float corner_radius = 0.0f;
    Thickness padding{};
};

class ControlTemplate : public RefCounted {
public:
    static Ref<ControlTemplate> Create() noexcept;

    ControlTemplate& SetPart(std::wstring name, TemplatePart part) noexcept {
        parts_.insert_or_assign(std::move(name), part);
        return *this;
    }

    const TemplatePart* Find(std::wstring_view name) const noexcept {
        auto found = parts_.find(std::wstring(name));
        return found == parts_.end() ? nullptr : &found->second;
    }

private:
    ControlTemplate() noexcept = default;

    std::unordered_map<std::wstring, TemplatePart> parts_;
};

}  // namespace nopeui

#endif  // NOPEUI_THEME_CONTROL_TEMPLATE_H_
