#ifndef NOPEUI_CONTROLS_DOCK_PANEL_H_
#define NOPEUI_CONTROLS_DOCK_PANEL_H_

#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"
#include "nopeui/layout/layout_types.h"

namespace nopeui {

// Docks children to the four edges in order. Each child takes the edge named by
// the attached Dock property and the rest of the slot passes to the next sibling.
// LastChildFill gives the final child whatever space is left.
class DockPanel : public Element {
public:
    static Ref<DockPanel> Create() noexcept;

    void Add(Ref<Element> child) noexcept;

    void SetLastChildFill(bool fill) noexcept;
    bool LastChildFill() const noexcept {
        return last_child_fill_;
    }

    std::wstring_view TypeName() const noexcept override {
        return L"DockPanel";
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
    DockPanel() noexcept = default;

    std::vector<Ref<Element>> children_;
    bool last_child_fill_ = true;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_DOCK_PANEL_H_
