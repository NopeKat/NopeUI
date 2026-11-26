#ifndef NOPEUI_CONTROLS_TOOLTIP_H_
#define NOPEUI_CONTROLS_TOOLTIP_H_

#include <string>

#include "nopeui/base/ref.h"
#include "nopeui/controls/label.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Compact popup that shows one line of helper text. The window owns its
// placement and lifetime.
class ToolTip : public Element {
public:
    static Ref<ToolTip> Create(std::wstring text) noexcept;

    ~ToolTip() override;

    std::wstring_view TypeName() const noexcept override {
        return L"ToolTip";
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    void CollectChildren(std::vector<Element*>& elements) noexcept override;
    AccessibleRole AccessibilityRole() const noexcept override;
    std::wstring AccessibilityName() const override;

private:
    ToolTip() noexcept = default;

    Ref<Label> label_;
    std::wstring text_;
};

// Owner-keyed tooltip text. The window walks ancestors so a tooltip set on a
// container also covers its children.
namespace ToolTipService {

void SetToolTip(Element* owner, std::wstring text);
std::wstring GetToolTip(const Element* owner);
void ClearToolTip(const Element* owner);

}  // namespace ToolTipService

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_TOOLTIP_H_
