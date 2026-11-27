#ifndef NOPEUI_CONTROLS_CANVAS_H_
#define NOPEUI_CONTROLS_CANVAS_H_

#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Absolute position container. Children are measured against an unbounded area
// and placed at Canvas.Left and Canvas.Top, both zero by default. The panel's
// desired size is the far edge of the content, so negative offsets are clipped
// like they would be in XAML.
class Canvas : public Element {
public:
    static Ref<Canvas> Create() noexcept;

    void Add(Ref<Element> child) noexcept;

    std::wstring_view TypeName() const noexcept override {
        return L"Canvas";
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    Element* HitTestOverride(Point point) noexcept override;
    void CollectFocusable(std::vector<Element*>& elements) noexcept override;
    void CollectChildren(std::vector<Element*>& elements) noexcept override;

private:
    Canvas() noexcept = default;

    std::vector<Ref<Element>> children_;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_CANVAS_H_
