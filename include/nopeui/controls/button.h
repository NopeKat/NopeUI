#ifndef NOPEUI_CONTROLS_BUTTON_H_
#define NOPEUI_CONTROLS_BUTTON_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "nopeui/base/event.h"
#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {
namespace graphics {
class TextRun;
}

// Clickable text button with normal, pointer over and pressed backgrounds.
class Button : public Element {
public:
    static Ref<Button> Create(std::wstring text) noexcept;

    Subscription SubscribeClick(Event<>::Handler handler) {
        return click_.Subscribe(std::move(handler));
    }

    // Keeps the subscription inside the button for the common single handler case.
    void SetOnClick(Event<>::Handler handler) {
        click_subscription_.Reset();
        click_subscription_ = click_.Subscribe(std::move(handler));
    }

    std::wstring_view TypeName() const noexcept override {
        return L"Button";
    }

    // Root of the instantiated visual tree template, or null when the button
    // still uses the default theme chrome or legacy named parts.
    Element* TemplateRoot() const noexcept {
        return template_root_.Get();
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    void OnPointerEnter() noexcept override;
    void OnPointerLeave() noexcept override;
    void OnPointerDown(Point point) noexcept override;
    void OnPointerUp(Point point) noexcept override;
    bool IsFocusable() const noexcept override;
    bool OnKeyDown(unsigned int key) noexcept override;
    bool OnKeyUp(unsigned int key) noexcept override;
    AccessibleRole AccessibilityRole() const noexcept override;
    std::wstring AccessibilityName() const override;
    bool HasAccessibilityInvoke() const noexcept override;
    void InvokeAccessibility() noexcept override;
    void ApplyControlTemplate(const ControlTemplate& control_template) noexcept override;
    const PropertyMetadata* ControlPropertyTable(std::size_t* count) const noexcept override;
    bool GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept override;
    bool SetControlProperty(std::wstring_view name, const PropertyValue& value) noexcept override;

private:
    Button() noexcept = default;

    std::wstring text_;
    std::unique_ptr<graphics::TextRun> run_;
    Event<> click_;
    Subscription click_subscription_;
    Ref<Element> template_root_;
    std::vector<Subscription> template_bindings_;
    std::uint32_t template_background_ = 0;
    std::uint32_t template_border_ = 0;
    std::uint32_t template_foreground_ = 0;
    float corner_radius_ = 4.0f;
    bool hovered_ = false;
    bool pressed_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_BUTTON_H_
