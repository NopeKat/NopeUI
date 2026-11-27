#ifndef NOPEUI_LAYOUT_ELEMENT_H_
#define NOPEUI_LAYOUT_ELEMENT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "nopeui/base/accessibility.h"
#include "nopeui/base/event.h"
#include "nopeui/base/geometry.h"
#include "nopeui/base/property_value.h"
#include "nopeui/base/ref.h"
#include "nopeui/layout/layout_types.h"
#include "nopeui/layout/property_trigger.h"

struct ID2D1RenderTarget;

namespace nopeui {

struct Theme;
class ControlTemplate;
struct AttachedPropertyMetadata;

// Minimal visual tree node: measure with an available size, arrange into final
// bounds, paint and hit test. Containers override HitTestOverride to route to
// children; leaf controls rely on the base bounds check.
//
// Measure takes the slot the parent offers and subtracts the margin before
// asking MeasureOverride for the content size, then adds the margin back to the
// desired size. Arrange takes a slot as well and places the content inside it
// according to Width/Height, Min/Max and the alignment pair. Containers therefore
// pass slots, not final rectangles.
class Element : public RefCounted {
public:
    using PropertyChangedEvent =
        Event<const PropertyMetadata&, const PropertyValue&, const PropertyValue&>;

    ~Element() override;

    void Measure(Size available) noexcept;
    void Arrange(const Rect& slot) noexcept;

    Size DesiredSize() const noexcept {
        return desired_;
    }

    Rect Bounds() const noexcept {
        return bounds_;
    }

    // Layout inputs. Zero Width/Height means "measure the content", zero
    // Min/Max means "no extra constraint" for Max and "zero" for Min.
    void SetMargin(Thickness margin) noexcept;
    Thickness Margin() const noexcept {
        return margin_;
    }
    void SetWidth(float width) noexcept;
    float Width() const noexcept {
        return width_;
    }
    void SetHeight(float height) noexcept;
    float Height() const noexcept {
        return height_;
    }
    void SetMinWidth(float width) noexcept;
    float MinWidth() const noexcept {
        return min_width_;
    }
    void SetMinHeight(float height) noexcept;
    float MinHeight() const noexcept {
        return min_height_;
    }
    void SetMaxWidth(float width) noexcept;
    float MaxWidth() const noexcept {
        return max_width_;
    }
    void SetMaxHeight(float height) noexcept;
    float MaxHeight() const noexcept {
        return max_height_;
    }
    void SetHorizontalAlignment(HorizontalAlignment alignment) noexcept;
    HorizontalAlignment GetHorizontalAlignment() const noexcept {
        return horizontal_alignment_;
    }
    void SetVerticalAlignment(VerticalAlignment alignment) noexcept;
    VerticalAlignment GetVerticalAlignment() const noexcept {
        return vertical_alignment_;
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

    // Metadata-backed properties. The base element contributes IsEnabled,
    // Foreground and the layout inputs; controls append their own tables.
    bool GetProperty(std::wstring_view name, PropertyValue* value) const noexcept;
    bool SetProperty(std::wstring_view name, const PropertyValue& value) noexcept;
    std::vector<const PropertyMetadata*> Properties() const;
    Subscription SubscribePropertyChanged(PropertyChangedEvent::Handler handler) {
        return property_changed_.Subscribe(std::move(handler));
    }

    // Style-sourced value: it gives way to a local value, so a style can never
    // override what the app set directly. The value is remembered even when a
    // local value wins, so ClearValue() falls back to the style rather than to
    // the metadata default.
    bool ApplyStyleProperty(std::wstring_view name, const PropertyValue& value) noexcept;
    bool HasLocalProperty(std::wstring_view name) const noexcept;

    // Drops a local value and falls back to the active trigger value, then the
    // style value, then the metadata default. Returns false for unknown names.
    bool ClearValue(std::wstring_view name) noexcept;

    // Local resources live on the element that declared them and are visible to
    // its descendants, matching the XAML Resources scoping rule.
    void SetResource(std::wstring name, PropertyValue value);
    const PropertyValue* FindLocalResource(std::wstring_view name) const noexcept;

    // Looks up a resource along the parent chain, then the theme palette
    // ("Accent", "Text", "SurfaceBorder", ...) which switches with the theme.
    bool FindResourceValue(std::wstring_view name, PropertyValue* value) const noexcept;

    // {ThemeResource Key} in markup: the value applies now when it resolves and
    // is re-applied by RefreshThemeResources() after a theme switch. Returns
    // false when the key is unknown or its type does not fit the property.
    bool BindThemeResource(std::wstring property, std::wstring key);
    void RefreshThemeResources();

    // Triggers come from a style. They re-evaluate whenever the watched property
    // changes; owner is the style pointer, used to replace a style's triggers.
    void AddTrigger(const void* owner, const PropertyTrigger& trigger) noexcept;
    void RefreshTriggers() noexcept;

    // Attached properties. Panels own the metadata and children store the
    // value, which is how Grid.Row reaches a child without a Grid field.
    bool SetAttachedProperty(
        const AttachedPropertyMetadata& metadata,
        const PropertyValue& value) noexcept;
    bool SetAttachedProperty(
        std::wstring_view name,
        const PropertyValue& value) noexcept;
    bool GetAttachedProperty(std::wstring_view name, PropertyValue* value) const noexcept;
    const PropertyValue* FindAttachedProperty(
        const AttachedPropertyMetadata& metadata) const noexcept;


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

    // Popup access for controls that own a menu (MenuBar). The root element gets
    // the handlers from Window; a control calls RequestPopup from anywhere in
    // the tree and the request walks up to the root.
    void SetPopupHandler(
        std::function<void(Ref<Element>, Point)> show,
        std::function<void()> close) noexcept;
    void RequestPopup(Ref<Element> popup, Point anchor);
    void RequestClosePopup();

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
    // adds IsEnabled, Foreground and the layout inputs on top.
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
    struct TriggerEntry {
        const void* owner = nullptr;
        PropertyTrigger trigger;
    };

    bool SetPropertyCore(
        std::wstring_view name,
        const PropertyValue& value,
        bool local) noexcept;
    Size ApplyMeasureConstraints(Size available) noexcept;
    Size ApplySizeConstraints(Size content) const noexcept;
    Rect ResolveSlot(const Rect& slot) const noexcept;
    void RecordLocalProperty(std::wstring_view name) noexcept;
    void EvaluateTriggeredProperty(std::wstring_view name) noexcept;
    void SetTriggerValue(std::wstring_view name, const PropertyValue& value) noexcept;
    void ClearTriggerValue(std::wstring_view name) noexcept;
    PropertyValue StyleOrDefault(std::wstring_view name) const noexcept;

    Element* parent_ = nullptr;
    const Theme* theme_ = nullptr;
    std::function<void()> invalidate_;
    std::function<void()> relayout_;
    std::function<void(Ref<Element>, Point)> popup_show_;
    std::function<void()> popup_close_;
    std::wstring accessibility_name_;
    std::wstring key_;
    Element* context_menu_ = nullptr;
    PropertyChangedEvent property_changed_;
    std::uint32_t foreground_ = 0;
    Thickness margin_{};
    float width_ = 0.0f;
    float height_ = 0.0f;
    float min_width_ = 0.0f;
    float min_height_ = 0.0f;
    float max_width_ = 0.0f;
    float max_height_ = 0.0f;
    Size content_desired_{};
    std::vector<std::pair<const AttachedPropertyMetadata*, PropertyValue>> attached_;
    std::vector<std::pair<std::wstring, PropertyValue>> resources_;
    std::vector<std::pair<std::wstring, std::wstring>> theme_bindings_;
    std::vector<TriggerEntry> triggers_;
    std::unordered_map<std::wstring, Subscription> trigger_subscriptions_;
    std::unordered_map<std::wstring, PropertyValue> trigger_values_;
    std::unordered_map<std::wstring, PropertyValue> style_values_;
    bool evaluating_trigger_ = false;
    std::vector<std::wstring> local_properties_;
    HorizontalAlignment horizontal_alignment_ = HorizontalAlignment::Stretch;
    VerticalAlignment vertical_alignment_ = VerticalAlignment::Stretch;
    bool focused_ = false;
    bool enabled_ = true;
    bool close_popup_on_click_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_LAYOUT_ELEMENT_H_
