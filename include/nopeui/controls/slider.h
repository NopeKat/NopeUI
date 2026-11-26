#ifndef NOPEUI_CONTROLS_SLIDER_H_
#define NOPEUI_CONTROLS_SLIDER_H_

#include <functional>
#include <utility>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Horizontal slider with pointer drag and keyboard steps.
class Slider : public Element {
public:
    static Ref<Slider> Create(
        double value = 0.0,
        double minimum = 0.0,
        double maximum = 1.0) noexcept;

    double Value() const noexcept {
        return value_;
    }
    void SetValue(double value) noexcept;

    void SetRange(double minimum, double maximum) noexcept;

    void SetOnValueChanged(std::function<void(double)> handler) {
        value_changed_ = std::move(handler);
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    void OnPointerEnter() noexcept override;
    void OnPointerLeave() noexcept override;
    void OnPointerDown(Point point) noexcept override;
    void OnPointerMove(Point point) noexcept override;
    void OnPointerUp(Point point) noexcept override;
    bool IsFocusable() const noexcept override;
    bool OnKeyDown(unsigned int key) noexcept override;
    AccessibleRole AccessibilityRole() const noexcept override;
    bool HasAccessibilityRange() const noexcept override;
    double AccessibilityRangeValue() const noexcept override;
    double AccessibilityRangeMinimum() const noexcept override;
    double AccessibilityRangeMaximum() const noexcept override;
    double AccessibilityRangeSmallChange() const noexcept override;
    double AccessibilityRangeLargeChange() const noexcept override;
    bool IsAccessibilityRangeReadOnly() const noexcept override;
    bool SetAccessibilityRangeValue(double value) override;

private:
    Slider() noexcept = default;

    float TrackLeft() const noexcept;
    float TrackRight() const noexcept;
    float ThumbCenter() const noexcept;
    void SetValueFromPoint(Point point) noexcept;
    double Step() const noexcept;

    double value_ = 0.0;
    double minimum_ = 0.0;
    double maximum_ = 1.0;
    std::function<void(double)> value_changed_;
    bool hovered_ = false;
    bool dragging_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_SLIDER_H_
