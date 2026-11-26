#ifndef NOPEUI_CONTROLS_BORDER_H_
#define NOPEUI_CONTROLS_BORDER_H_

#include <cstdint>

#include "nopeui/base/geometry.h"
#include "nopeui/base/property.h"
#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Draws a background and an optional one pixel outline around a single child.
class Border : public Element {
public:
    static Ref<Border> Create() noexcept;

    void SetChild(Ref<Element> child) noexcept;
    void SetPadding(Thickness padding) noexcept;

    // By default the background follows the theme. UseSurfaceBackground(true)
    // picks the card surface instead of the window background.
    void UseSurfaceBackground(bool surface) noexcept;
    void SetBackground(std::uint32_t color) noexcept;
    void SetBorderColor(std::uint32_t color) noexcept;
    void SetBorderVisible(bool visible) noexcept;

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    Element* HitTestOverride(Point point) noexcept override;
    void CollectFocusable(std::vector<Element*>& elements) noexcept override;

private:
    Border() noexcept = default;

    Ref<Element> child_;
    Thickness padding_;
    Property<std::uint32_t> background_{0};
    Property<std::uint32_t> border_color_{0};
    bool use_theme_background_ = true;
    bool surface_ = false;
    bool border_visible_ = true;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_BORDER_H_
