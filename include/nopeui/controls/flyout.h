#ifndef NOPEUI_CONTROLS_FLYOUT_H_
#define NOPEUI_CONTROLS_FLYOUT_H_

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Lightweight anchored panel. It hosts one content element and paints the
// popup surface; the window owns placement, dismissal and focus.
class Flyout : public Element {
public:
    static Ref<Flyout> Create() noexcept;

    ~Flyout() override;

    void SetContent(Ref<Element> content) noexcept;

    std::wstring_view TypeName() const noexcept override {
        return L"Flyout";
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    void CollectChildren(std::vector<Element*>& elements) noexcept override;
    AccessibleRole AccessibilityRole() const noexcept override;

private:
    Flyout() noexcept = default;

    Ref<Element> content_;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_FLYOUT_H_
