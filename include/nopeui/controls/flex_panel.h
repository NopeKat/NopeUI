#ifndef NOPEUI_CONTROLS_FLEX_PANEL_H_
#define NOPEUI_CONTROLS_FLEX_PANEL_H_

#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

enum class FlexDirection {
    Row,
    Column,
};

// Arranges children along one axis and hands leftover space to children that
// ask for it through the FlexGrow attached property. With Wrap on, children
// move to a new line once the main axis runs out, so the panel measures against
// the offered width (row) or height (column) instead of growing without bound.
class FlexPanel : public Element {
public:
    static Ref<FlexPanel> Create(float gap = 8.0f) noexcept;

    void Add(Ref<Element> child) noexcept;
    void SetDirection(FlexDirection direction) noexcept;
    FlexDirection GetDirection() const noexcept {
        return direction_;
    }
    void SetGap(float gap) noexcept;
    float Gap() const noexcept {
        return gap_;
    }
    void SetWrap(bool wrap) noexcept;
    bool Wrap() const noexcept {
        return wrap_;
    }

    std::wstring_view TypeName() const noexcept override {
        return L"FlexPanel";
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
    FlexPanel() noexcept = default;

    struct Line {
        std::vector<Element*> items;
        float main = 0.0f;
        float cross = 0.0f;
    };

    bool IsRow() const noexcept {
        return direction_ == FlexDirection::Row;
    }
    float GrowOf(const Element& child) const noexcept;
    std::vector<Line> BuildLines(float main_limit) noexcept;

    float gap_ = 8.0f;
    FlexDirection direction_ = FlexDirection::Row;
    bool wrap_ = false;
    std::vector<Ref<Element>> children_;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_FLEX_PANEL_H_
