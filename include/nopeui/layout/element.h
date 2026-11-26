#ifndef NOPEUI_LAYOUT_ELEMENT_H_
#define NOPEUI_LAYOUT_ELEMENT_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nopeui/base/accessibility.h"
#include "nopeui/base/geometry.h"
#include "nopeui/base/ref.h"

struct ID2D1RenderTarget;

namespace nopeui {

struct Theme;

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

    // The root element receives the theme pointer from Window; children inherit
    // it by walking up the parent chain.
    void SetTheme(const Theme* theme) noexcept {
        theme_ = theme;
    }
    const Theme& Colors() const noexcept;

    // Only meaningful on the root element: lets a control request a repaint
    // without knowing about the window.
    void SetInvalidateHandler(std::function<void()> handler) noexcept {
        invalidate_ = std::move(handler);
    }
    void Invalidate() noexcept;

    // Requests measure and arrange again, then a repaint.
    void SetRelayoutHandler(std::function<void()> handler) noexcept {
        relayout_ = std::move(handler);
    }
    void InvalidateLayout() noexcept;

    // Focus and keyboard. IsFocusable() marks participation in the Tab order;
    // CollectFocusable() walks the tree in visual order.
    virtual bool IsFocusable() const noexcept {
        return false;
    }
    virtual void CollectFocusable(std::vector<Element*>& elements) noexcept;

    // Accessibility metadata and automation actions. Controls override the
    // parts they support; the window maps them to UI Automation properties.
    virtual AccessibleRole AccessibilityRole() const noexcept {
        return AccessibleRole::Pane;
    }
    virtual std::wstring AccessibilityName() const {
        return {};
    }
    virtual std::wstring AccessibilityValue() const {
        return {};
    }
    virtual bool SetAccessibilityValue(std::wstring_view) {
        return false;
    }
    virtual bool HasAccessibilityValue() const noexcept {
        return false;
    }
    virtual bool HasAccessibilityInvoke() const noexcept {
        return false;
    }
    virtual bool HasAccessibilityToggle() const noexcept {
        return false;
    }
    virtual bool AccessibilityToggleOn() const noexcept {
        return false;
    }
    virtual void InvokeAccessibility() noexcept {}
    virtual void ToggleAccessibility() noexcept {}

    // Child order for the automation tree.
    virtual void CollectChildren(std::vector<Element*>&) noexcept {}

    bool IsFocused() const noexcept {
        return focused_;
    }
    void SetFocused(bool focused) noexcept {
        focused_ = focused;
    }

    bool IsEnabled() const noexcept {
        return enabled_;
    }
    void SetEnabled(bool enabled) noexcept;

    // Lets a scrollable ancestor adjust its offset when focus moves.
    virtual void BringChildIntoView(const Element*) noexcept {}

    virtual bool OnKeyDown(unsigned int) noexcept {
        return false;
    }
    virtual bool OnKeyUp(unsigned int) noexcept {
        return false;
    }
    // Printable character produced by the host after key translation.
    virtual bool OnChar(wchar_t) noexcept {
        return false;
    }

    // IME composition starts, updates and commits. The composition text is a
    // preview; only the result belongs to the document.
    virtual void OnImeStart() noexcept {}
    virtual void OnImeComposition(std::wstring_view) noexcept {}
    virtual void OnImeResult(std::wstring_view) noexcept {}
    virtual void OnImeEnd() noexcept {}

    // Client-space point used to place the IME composition window.
    virtual Point ImeCaretPoint() const noexcept {
        return Point{bounds_.x, bounds_.y + bounds_.height};
    }

    // Wheel input walks from the hit element up to the nearest scrollable
    // ancestor. Returns true when one of them handled the delta.
    virtual bool OnWheel(int) noexcept {
        return false;
    }
    bool BubbleWheel(int delta) noexcept;

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

    // Containers clip their children to their own bounds before painting.
    void PushContentClip(ID2D1RenderTarget* target) noexcept;
    void PopContentClip(ID2D1RenderTarget* target) noexcept;

    bool Contains(Point point) const noexcept;

    Rect bounds_{};
    Size desired_{};

private:
    Element* parent_ = nullptr;
    const Theme* theme_ = nullptr;
    std::function<void()> invalidate_;
    std::function<void()> relayout_;
    bool focused_ = false;
    bool enabled_ = true;
};

}  // namespace nopeui

#endif  // NOPEUI_LAYOUT_ELEMENT_H_
