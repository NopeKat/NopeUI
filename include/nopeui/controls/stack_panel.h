#ifndef NOPEUI_CONTROLS_STACK_PANEL_H_
#define NOPEUI_CONTROLS_STACK_PANEL_H_

#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

enum class Orientation {
    Vertical,
    Horizontal,
};

// Stacks children along one axis and offsets each by the configured spacing.
class StackPanel : public Element {
public:
    static Ref<StackPanel> Create(float spacing = 12.0f) noexcept;

    void Add(Ref<Element> child) noexcept;
    void SetOrientation(Orientation orientation) noexcept;
    void SetSpacing(float spacing) noexcept;
    Orientation GetOrientation() const noexcept {
        return orientation_;
    }

    std::wstring_view TypeName() const noexcept override {
        return L"StackPanel";
    }

    const std::vector<Ref<Element>>& Children() const noexcept {
        return children_;
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    Element* HitTestOverride(Point point) noexcept override;
    void CollectFocusable(std::vector<Element*>& elements) noexcept override;
    void CollectChildren(std::vector<Element*>& elements) noexcept override;
    const PropertyMetadata* ControlPropertyTable(std::size_t* count) const noexcept override;
    bool GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept override;
    bool SetControlProperty(std::wstring_view name, const PropertyValue& value) noexcept override;

private:
    StackPanel() noexcept = default;

    float spacing_ = 12.0f;
    Orientation orientation_ = Orientation::Vertical;
    std::vector<Ref<Element>> children_;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_STACK_PANEL_H_
