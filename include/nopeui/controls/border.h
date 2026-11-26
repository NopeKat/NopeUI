#ifndef NOPEUI_CONTROLS_BORDER_H_
#define NOPEUI_CONTROLS_BORDER_H_

#include <cstdint>

#include "nopeui/base/geometry.h"
#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Draws a background and an optional one pixel outline around a single child.
class Border : public Element {
public:
    static Ref<Border> Create() noexcept;

    void SetChild(Ref<Element> child) noexcept;
    void SetPadding(Thickness padding) noexcept;
    void SetBackground(std::uint32_t color) noexcept;
    void SetBorderColor(std::uint32_t color) noexcept;

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    Element* HitTestOverride(Point point) noexcept override;

private:
    Border() noexcept = default;

    Ref<Element> child_;
    Thickness padding_;
    std::uint32_t background_ = 0xFFFFFF;
    std::uint32_t border_color_ = 0xE1E1E1;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_BORDER_H_
