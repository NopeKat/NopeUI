#ifndef NOPEUI_CONTROLS_CONTEXT_MENU_H_
#define NOPEUI_CONTROLS_CONTEXT_MENU_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/graphics/text.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Small drop-down action list shown by the window on right click. Items are
// data rows rather than a visual tree so the menu stays cheap to open.
class ContextMenu : public Element {
public:
    static Ref<ContextMenu> Create() noexcept;

    void AddItem(std::wstring text, std::function<void()> handler) noexcept;

    std::wstring_view TypeName() const noexcept override {
        return L"ContextMenu";
    }

    void CollectAccessibleMenuItems(std::vector<std::wstring>& items) const noexcept override;

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    void OnPointerEnter() noexcept override;
    void OnPointerLeave() noexcept override;
    void OnPointerMove(Point point) noexcept override;
    void OnPointerDown(Point point) noexcept override;
    void OnPointerUp(Point point) noexcept override;
    AccessibleRole AccessibilityRole() const noexcept override;
    std::wstring AccessibilityName() const override;

private:
    struct Item {
        std::wstring text;
        std::unique_ptr<graphics::TextRun> run;
        std::function<void()> handler;
    };

    ContextMenu() noexcept = default;

    int ItemAt(Point point) const noexcept;

    std::vector<Item> items_;
    std::uint32_t hovered_ = 0xffffffffu;
    bool pressed_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_CONTEXT_MENU_H_
