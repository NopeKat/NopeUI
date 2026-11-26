#ifndef NOPEUI_LAYOUT_ELEMENT_H_
#define NOPEUI_LAYOUT_ELEMENT_H_

#include <cstdint>

#include "nopeui/base/geometry.h"
#include "nopeui/base/ref.h"

struct ID2D1RenderTarget;

namespace nopeui {

// Minimal visual tree node: measure with an available size, arrange into final
// bounds, paint and hit test. Containers override HitTestOverride to route to
// children; leaf controls rely on the base bounds check.
class Element : public RefCounted {
public:
    ~Element() override = default;

    void Measure(Size available) noexcept;
    void Arrange(const Rect& bounds) noexcept;

    Size DesiredSize() const noexcept {
        return desired_;
    }

    Rect Bounds() const noexcept {
        return bounds_;
    }

    Element* Parent() const noexcept {
        return parent_;
    }

    Element* HitTest(Point point) noexcept;
    void Paint(ID2D1RenderTarget* target) noexcept;

    void SetParent(Element* parent) noexcept {
        parent_ = parent;
    }

    // Pointer dispatch, driven by the host on the element returned by HitTest.
    virtual void OnPointerEnter() noexcept {}
    virtual void OnPointerLeave() noexcept {}
    virtual void OnPointerMove(Point) noexcept {}
    virtual void OnPointerDown(Point) noexcept {}
    virtual void OnPointerUp(Point) noexcept {}

protected:
    Element() noexcept = default;

    virtual Size MeasureOverride(Size available) = 0;
    virtual Size ArrangeOverride(Size final_size) noexcept;
    virtual void PaintContent(ID2D1RenderTarget* target) = 0;
    virtual Element* HitTestOverride(Point point) noexcept;

    bool Contains(Point point) const noexcept;

    Rect bounds_{};
    Size desired_{};

private:
    Element* parent_ = nullptr;
};

}  // namespace nopeui

#endif  // NOPEUI_LAYOUT_ELEMENT_H_
