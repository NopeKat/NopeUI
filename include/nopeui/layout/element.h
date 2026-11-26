#ifndef NOPEUI_LAYOUT_ELEMENT_H_
#define NOPEUI_LAYOUT_ELEMENT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nopeui/base/accessibility.h"
#include "nopeui/base/event.h"
#include "nopeui/base/geometry.h"
#include "nopeui/base/property_value.h"
#include "nopeui/base/ref.h"

struct ID2D1RenderTarget;

namespace nopeui {

struct Theme;
class ControlTemplate;

// Minimal visual tree node: measure with an available size, arrange into final
// bounds, paint and hit test. Containers override HitTestOverride to route to
// children; leaf controls rely on the base bounds check.
class Element : public RefCounted {
public:
    using PropertyChangedEvent =
        Event<const PropertyMetadata&, const PropertyValue&, const PropertyValue&>;

    ~Element() override;

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

    // Stable type name used by style selectors and diagnostics.
    virtual std::wstring_view TypeName() const noexcept {
        return L"Element";
    }

    // Optional key for keyed styles within one type.
    void SetKey(std::wstring key) noexcept {
        key_ = std::move(key);
    }
    const std::wstring& Key() const noexcept {
        return key_;
    }

    // Inherited foreground override. Zero means inherit from the parent, then
    // fall back to the theme text color at the paint site.
    void SetForeground(std::uint32_t color) noexcept;
    std::uint32_t Foreground() const noexcept;

    // Metadata-backed properties. The base element contributes IsEnabled and
    // Foreground; controls append their own tables.
    bool GetProperty(std::wstring_view name, PropertyValue* value) const noexcept;
    bool SetProperty(std::wstring_view name, const PropertyValue& value) noexcept;
    std::vector<const PropertyMetadata*> Properties() const;
    Subscription SubscribePropertyChanged(PropertyChangedEvent::Handler handler) {
        return property_changed_.Subscribe(std::move(handler));
    }

    // Applies a template to this control. Controls that support named parts
    // override this and copy the parts they consume.
    virtual void ApplyControlTemplate(const ControlTemplate&) noexcept {}

    // Right-click menu shown by the window when this element is hit. The menu
    // stays alive through the element's reference after the popup closes.
    void SetContextMenu(Ref<Element> menu) noexcept {
        Element* next = menu.Get();
        if (next == context_menu_) {
            return;
        }
        if (next != nullptr) {
            next->AddRef();
        }
        if (context_menu_ != nullptr) {
            context_menu_->Release();
        }
        context_menu_ = next;
    }
    Element* GetContextMenu() const noexcept {
        return context_menu_;
    }

    // Popup controls such as menus can ask the window to dismiss the popup
    // after the click that activated them has finished.
    void SetClosePopupOnClick(bool close) noexcept {
        close_popup_on_click_ = close;
    }
    bool ClosePopupOnClick() const noexcept {
        return close_popup_on_click_;
    }

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
    void SetAccessibilityName(std::wstring name) {
        accessibility_name_ = std::move(name);
    }
    virtual AccessibleRole AccessibilityRole() const noexcept {
        return AccessibleRole::Pane;
    }
    virtual std::wstring AccessibilityName() const {
        return accessibility_name_;
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

    // Radio buttons in one parent form a group; a newly selected button asks
    // its siblings to clear their own state.
    virtual void DeselectRadioGroup(const Element*) noexcept {}

    // Text pattern support. Offsets are UTF-16 code units in a single-line
    // document, matching the text box storage.
    virtual bool HasAccessibilityText() const noexcept {
        return false;
    }
    virtual std::wstring AccessibilityText() const {
        return {};
    }
    virtual int AccessibilitySelectionStart() const noexcept {
        return 0;
    }
    virtual int AccessibilitySelectionEnd() const noexcept {
        return 0;
    }
    virtual bool SetAccessibilitySelection(int, int) {
        return false;
    }

    // Range value support for sliders and progress bars.
    virtual bool HasAccessibilityRange() const noexcept {
        return false;
    }
    virtual double AccessibilityRangeValue() const noexcept {
        return 0.0;
    }
    virtual double AccessibilityRangeMinimum() const noexcept {
        return 0.0;
    }
    virtual double AccessibilityRangeMaximum() const noexcept {
        return 0.0;
    }
    virtual double AccessibilityRangeSmallChange() const noexcept {
        return 0.0;
    }
    virtual double AccessibilityRangeLargeChange() const noexcept {
        return 0.0;
    }
    virtual bool IsAccessibilityRangeReadOnly() const noexcept {
        return true;
    }
    virtual bool SetAccessibilityRangeValue(double) {
        return false;
    }

    // Scroll pattern support. Percent and view size follow UIA conventions:
    // 0 to 100, with -1 for an axis that cannot scroll.
    virtual bool HasAccessibilityScroll() const noexcept {
        return false;
    }
    virtual bool AccessibilityHorizontallyScrollable() const noexcept {
        return false;
    }
    virtual bool AccessibilityVerticallyScrollable() const noexcept {
        return false;
    }
    virtual double AccessibilityHorizontalScrollPercent() const noexcept {
        return -1.0;
    }
    virtual double AccessibilityVerticalScrollPercent() const noexcept {
        return -1.0;
    }
    virtual double AccessibilityHorizontalViewSize() const noexcept {
        return 100.0;
    }
    virtual double AccessibilityVerticalViewSize() const noexcept {
        return 100.0;
    }
    virtual bool SetAccessibilityScrollPercent(double, double) noexcept {
        return false;
    }
    virtual bool ScrollAccessibilityHorizontal(double) noexcept {
        return false;
    }
    virtual bool ScrollAccessibilityVertical(double) noexcept {
        return false;
    }

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

    // Controls expose their own property descriptors here. The base element
    // adds IsEnabled and Foreground on top.
    virtual const PropertyMetadata* ControlPropertyTable(std::size_t* count) const noexcept;
    virtual bool GetControlProperty(std::wstring_view, PropertyValue*) const noexcept;
    virtual bool SetControlProperty(std::wstring_view, const PropertyValue&) noexcept;

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
    std::wstring accessibility_name_;
    std::wstring key_;
    Element* context_menu_ = nullptr;
    PropertyChangedEvent property_changed_;
    std::uint32_t foreground_ = 0;
    bool focused_ = false;
    bool enabled_ = true;
    bool close_popup_on_click_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_LAYOUT_ELEMENT_H_
