#ifndef NOPEUI_CONTROLS_LABEL_H_
#define NOPEUI_CONTROLS_LABEL_H_

#include <cstdint>
#include <memory>
#include <string>

#include "nopeui/base/property.h"
#include "nopeui/base/ref.h"
#include "nopeui/graphics/text.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Text block. With wrapping enabled it measures against the available width
// and grows in height; ellipsis trims a single line to the available width.
class Label : public Element {
public:
    static Ref<Label> Create(
        std::wstring text,
        graphics::TextStyle style = {}) noexcept;

    void SetTextStyle(graphics::TextStyle style) noexcept;

    // Zero means the theme text color.
    void SetColor(std::uint32_t color) noexcept;

    std::wstring_view TypeName() const noexcept override {
        return L"Label";
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    AccessibleRole AccessibilityRole() const noexcept override;
    std::wstring AccessibilityName() const override;
    const PropertyMetadata* ControlPropertyTable(std::size_t* count) const noexcept override;
    bool GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept override;
    bool SetControlProperty(std::wstring_view name, const PropertyValue& value) noexcept override;

private:
    Label() noexcept = default;

    void RebuildRun(float width) noexcept;

    std::wstring text_;
    std::unique_ptr<graphics::TextRun> run_;
    graphics::TextStyle style_;
    Property<std::uint32_t> color_{0};
    float run_width_ = 0.0f;
    bool run_dirty_ = true;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_LABEL_H_
