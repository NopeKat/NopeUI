#ifndef NOPEUI_CONTROLS_MENU_BAR_H_
#define NOPEUI_CONTROLS_MENU_BAR_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {
namespace graphics {
class TextRun;
}

// One entry of a drop-down menu.
struct MenuCommand {
    std::wstring text;
    std::function<void()> handler;
};

// Top-level menu strip. Each title opens a drop-down in the window popup layer,
// so the menu is not part of the content layout and closes on outside clicks.
class MenuBar : public Element {
public:
    static Ref<MenuBar> Create() noexcept;

    void AddMenu(std::wstring title, std::vector<MenuCommand> commands);

    std::size_t MenuCount() const noexcept {
        return items_.size();
    }

    std::wstring_view TypeName() const noexcept override {
        return L"MenuBar";
    }

    int ActiveMenuIndex() const noexcept {
        return active_;
    }

    void CollectAccessibleMenuItems(std::vector<std::wstring>& items) const noexcept override;
    Rect AccessibilityMenuItemBounds(std::size_t index) const noexcept override;
    bool InvokeAccessibilityMenuItem(std::size_t index) noexcept override;
    bool HasAccessibilityMenuItemExpandCollapse() const noexcept override;
    bool AccessibilityMenuItemExpanded(std::size_t index) const noexcept override;
    bool SetAccessibilityMenuItemExpanded(std::size_t index, bool expanded) noexcept override;

protected:
    Size MeasureOverride(Size available) noexcept override;
    Size ArrangeOverride(Size final_size) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    void OnPointerMove(Point point) noexcept override;
    void OnPointerLeave() noexcept override;
    void OnPointerDown(Point point) noexcept override;
    void OnPointerUp(Point point) noexcept override;
    bool IsFocusable() const noexcept override;
    bool OnKeyDown(unsigned int key) noexcept override;
    AccessibleRole AccessibilityRole() const noexcept override;
    std::wstring AccessibilityName() const override;

private:
    MenuBar() noexcept = default;

    struct Item {
        std::wstring title;
        std::unique_ptr<graphics::TextRun> run;
        std::vector<MenuCommand> commands;
    };

    int ItemAt(Point point) const noexcept;
    void OpenMenu(std::size_t index) noexcept;

    std::vector<Item> items_;
    int hovered_ = -1;
    int active_ = -1;
    int pressed_ = -1;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_MENU_BAR_H_
