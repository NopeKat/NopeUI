#ifndef NOPEUI_CONTROLS_PROGRESS_BAR_H_
#define NOPEUI_CONTROLS_PROGRESS_BAR_H_

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Determinate or indeterminate progress bar. Value is normalized to 0..1.
class ProgressBar : public Element {
public:
    static Ref<ProgressBar> Create(float value = 0.0f) noexcept;

    float Value() const noexcept {
        return value_;
    }
    void SetValue(float value) noexcept;

    bool IsIndeterminate() const noexcept {
        return indeterminate_;
    }
    void SetIndeterminate(bool indeterminate) noexcept;

    std::wstring_view TypeName() const noexcept override {
        return L"ProgressBar";
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    AccessibleRole AccessibilityRole() const noexcept override;
    bool HasAccessibilityRange() const noexcept override;
    double AccessibilityRangeValue() const noexcept override;
    double AccessibilityRangeMinimum() const noexcept override;
    double AccessibilityRangeMaximum() const noexcept override;
    double AccessibilityRangeSmallChange() const noexcept override;
    double AccessibilityRangeLargeChange() const noexcept override;
    bool IsAccessibilityRangeReadOnly() const noexcept override;
    const PropertyMetadata* ControlPropertyTable(std::size_t* count) const noexcept override;
    bool GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept override;
    bool SetControlProperty(std::wstring_view name, const PropertyValue& value) noexcept override;

private:
    ProgressBar() noexcept = default;

    float value_ = 0.0f;
    bool indeterminate_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_PROGRESS_BAR_H_
