#include "nopeui/platform/automation.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <oleauto.h>
#include <uiautomation.h>
#include <uiautomationcoreapi.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "nopeui/layout/element.h"
#include "nopeui/platform/window.h"

namespace nopeui {
namespace {

PROPERTYID ControlTypeForRole(AccessibleRole role) noexcept {
    switch (role) {
        case AccessibleRole::Text:
            return UIA_TextControlTypeId;
        case AccessibleRole::Button:
            return UIA_ButtonControlTypeId;
        case AccessibleRole::CheckBox:
            return UIA_CheckBoxControlTypeId;
        case AccessibleRole::Edit:
            return UIA_EditControlTypeId;
        case AccessibleRole::Group:
            return UIA_GroupControlTypeId;
        case AccessibleRole::Window:
            return UIA_WindowControlTypeId;
        case AccessibleRole::Pane:
            break;
    }
    return UIA_PaneControlTypeId;
}

void VariantString(VARIANT* value, const std::wstring& text) {
    VariantInit(value);
    value->vt = VT_BSTR;
    value->bstrVal = SysAllocStringLen(
        text.c_str(),
        static_cast<UINT>(text.size()));
}

void VariantInt(VARIANT* value, int number) {
    VariantInit(value);
    value->vt = VT_I4;
    value->lVal = number;
}

void VariantBool(VARIANT* value, bool flag) {
    VariantInit(value);
    value->vt = VT_BOOL;
    value->boolVal = flag ? VARIANT_TRUE : VARIANT_FALSE;
}

void VariantRect(VARIANT* value, const UiaRect& rect) {
    VariantInit(value);
    SAFEARRAY* array = SafeArrayCreateVector(VT_R8, 0, 4);
    if (array == nullptr) {
        return;
    }
    double coordinates[4] = {
        rect.left,
        rect.top,
        rect.width,
        rect.height};
    for (LONG index = 0; index < 4; ++index) {
        SafeArrayPutElement(array, &index, &coordinates[index]);
    }
    value->vt = VT_ARRAY | VT_R8;
    value->parray = array;
}

bool ElementScreenRect(Window* window, Element* element, UiaRect* rect) {
    if (window == nullptr || element == nullptr || rect == nullptr) {
        return false;
    }
    POINT origin{0, 0};
    if (ClientToScreen(window->Handle(), &origin) == FALSE) {
        return false;
    }
    const Rect bounds = element->Bounds();
    rect->left = static_cast<double>(origin.x) + bounds.x;
    rect->top = static_cast<double>(origin.y) + bounds.y;
    rect->width = bounds.width;
    rect->height = bounds.height;
    return true;
}

bool WindowScreenRect(Window* window, UiaRect* rect) {
    if (window == nullptr || rect == nullptr) {
        return false;
    }
    RECT client{};
    if (GetClientRect(window->Handle(), &client) == FALSE) {
        return false;
    }
    POINT origin{0, 0};
    if (ClientToScreen(window->Handle(), &origin) == FALSE) {
        return false;
    }
    rect->left = origin.x;
    rect->top = origin.y;
    rect->width = client.right;
    rect->height = client.bottom;
    return true;
}

SAFEARRAY* CreateRuntimeId(int identifier) {
    SAFEARRAY* array = SafeArrayCreateVector(VT_I4, 0, 2);
    if (array == nullptr) {
        return nullptr;
    }
    int values[2] = {UiaAppendRuntimeId, identifier};
    for (LONG index = 0; index < 2; ++index) {
        SafeArrayPutElement(array, &index, &values[index]);
    }
    return array;
}

int RuntimeIdFor(Element* element) {
    return static_cast<int>(
        reinterpret_cast<std::uintptr_t>(element) & 0x7fffffffU);
}

std::wstring WindowTitle(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) {
        return L"NopeUI";
    }
    std::wstring title(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(hwnd, title.data(), length + 1);
    title.resize(static_cast<std::size_t>(length));
    return title;
}

class ElementProvider;

IRawElementProviderFragment* MakeElementProvider(
    Window* window,
    Element* element,
    IRawElementProviderFragmentRoot* root);

class WindowProvider final : public IRawElementProviderSimple,
                             public IRawElementProviderFragment,
                             public IRawElementProviderFragmentRoot {
public:
    explicit WindowProvider(Window* window) noexcept : window_(window) {}

    void Detach() noexcept {
        window_ = nullptr;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (object == nullptr) {
            return E_INVALIDARG;
        }
        *object = nullptr;
        if (iid == __uuidof(IRawElementProviderSimple)) {
            *object = static_cast<IRawElementProviderSimple*>(this);
        } else if (iid == __uuidof(IRawElementProviderFragment)) {
            *object = static_cast<IRawElementProviderFragment*>(this);
        } else if (iid == __uuidof(IRawElementProviderFragmentRoot)) {
            *object = static_cast<IRawElementProviderFragmentRoot*>(this);
        } else if (iid == __uuidof(IUnknown)) {
            *object = static_cast<IRawElementProviderSimple*>(this);
        }
        if (*object == nullptr) {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&references_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const LONG remaining = InterlockedDecrement(&references_);
        if (remaining == 0) {
            delete this;
        }
        return static_cast<ULONG>(remaining);
    }

    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* options) override {
        if (options == nullptr) {
            return E_INVALIDARG;
        }
        *options = ProviderOptions_ServerSideProvider;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID, IUnknown** provider) override {
        if (provider == nullptr) {
            return E_INVALIDARG;
        }
        *provider = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID property, VARIANT* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        VariantInit(value);
        if (window_ == nullptr) {
            return S_OK;
        }
        switch (property) {
            case UIA_NamePropertyId:
                VariantString(value, WindowTitle(window_->Handle()));
                break;
            case UIA_ControlTypePropertyId:
                VariantInt(value, UIA_WindowControlTypeId);
                break;
            case UIA_IsEnabledPropertyId:
                VariantBool(value, true);
                break;
            case UIA_IsKeyboardFocusablePropertyId:
                VariantBool(value, true);
                break;
            case UIA_HasKeyboardFocusPropertyId:
                VariantBool(value, false);
                break;
            case UIA_IsControlElementPropertyId:
            case UIA_IsContentElementPropertyId:
                VariantBool(value, true);
                break;
            case UIA_IsOffscreenPropertyId:
                VariantBool(value, false);
                break;
            case UIA_AutomationIdPropertyId:
                VariantString(value, L"NopeUI.Window");
                break;
            case UIA_ClassNamePropertyId:
                VariantString(value, L"NopeUI.Window");
                break;
            case UIA_FrameworkIdPropertyId:
                VariantString(value, L"NopeUI");
                break;
            case UIA_ProcessIdPropertyId:
                VariantInt(value, static_cast<int>(GetCurrentProcessId()));
                break;
            case UIA_NativeWindowHandlePropertyId:
                VariantInt(
                    value,
                    static_cast<int>(reinterpret_cast<INT_PTR>(window_->Handle())));
                break;
            case UIA_BoundingRectanglePropertyId: {
                UiaRect rect{};
                if (WindowScreenRect(window_, &rect)) {
                    VariantRect(value, rect);
                }
                break;
            }
            default:
                break;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
        IRawElementProviderSimple** provider) override {
        if (provider == nullptr) {
            return E_INVALIDARG;
        }
        *provider = nullptr;
        if (window_ == nullptr) {
            return S_OK;
        }
        return UiaHostProviderFromHwnd(window_->Handle(), provider);
    }

    HRESULT STDMETHODCALLTYPE Navigate(
        NavigateDirection direction,
        IRawElementProviderFragment** provider) override {
        if (provider == nullptr) {
            return E_INVALIDARG;
        }
        *provider = nullptr;
        if (window_ == nullptr) {
            return S_OK;
        }
        Element* content = window_->ContentElement();
        if (content == nullptr) {
            return S_OK;
        }
        if (direction == NavigateDirection_FirstChild ||
            direction == NavigateDirection_LastChild) {
            *provider = MakeElementProvider(window_, content, this);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** runtime_id) override {
        if (runtime_id == nullptr) {
            return E_INVALIDARG;
        }
        *runtime_id = CreateRuntimeId(0);
        return *runtime_id != nullptr ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* rect) override {
        if (rect == nullptr) {
            return E_INVALIDARG;
        }
        UiaRect bounds{};
        WindowScreenRect(window_, &bounds);
        *rect = bounds;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** roots) override {
        if (roots == nullptr) {
            return E_INVALIDARG;
        }
        *roots = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetFocus() override {
        if (window_ == nullptr) {
            return S_OK;
        }
        ::SetFocus(window_->Handle());
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_FragmentRoot(
        IRawElementProviderFragmentRoot** root) override {
        if (root == nullptr) {
            return E_INVALIDARG;
        }
        *root = this;
        AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(
        double x,
        double y,
        IRawElementProviderFragment** provider) override {
        if (provider == nullptr) {
            return E_INVALIDARG;
        }
        *provider = nullptr;
        if (window_ == nullptr) {
            return S_OK;
        }
        Element* content = window_->ContentElement();
        if (content == nullptr) {
            return S_OK;
        }
        POINT point{static_cast<LONG>(x), static_cast<LONG>(y)};
        ScreenToClient(window_->Handle(), &point);
        Element* hit = content->HitTest(Point{
            static_cast<float>(point.x),
            static_cast<float>(point.y)});
        if (hit != nullptr) {
            *provider = MakeElementProvider(window_, hit, this);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetFocus(
        IRawElementProviderFragment** provider) override {
        if (provider == nullptr) {
            return E_INVALIDARG;
        }
        *provider = nullptr;
        if (window_ == nullptr || window_->FocusedElement() == nullptr) {
            return S_OK;
        }
        *provider = MakeElementProvider(
            window_,
            window_->FocusedElement(),
            this);
        return S_OK;
    }

    LONG references_ = 1;
    Window* window_ = nullptr;
};

class ElementProvider final : public IRawElementProviderSimple,
                              public IRawElementProviderFragment,
                              public IInvokeProvider,
                              public IToggleProvider,
                              public IValueProvider {
public:
    ElementProvider(
        Window* window,
        Element* element,
        IRawElementProviderFragmentRoot* root) noexcept
        : window_(window), element_(element), root_(root) {
        if (root_ != nullptr) {
            root_->AddRef();
        }
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (object == nullptr) {
            return E_INVALIDARG;
        }
        *object = nullptr;
        if (iid == __uuidof(IRawElementProviderSimple)) {
            *object = static_cast<IRawElementProviderSimple*>(this);
        } else if (iid == __uuidof(IRawElementProviderFragment)) {
            *object = static_cast<IRawElementProviderFragment*>(this);
        } else if (iid == __uuidof(IInvokeProvider) &&
                   element_ != nullptr && element_->HasAccessibilityInvoke()) {
            *object = static_cast<IInvokeProvider*>(this);
        } else if (iid == __uuidof(IToggleProvider) &&
                   element_ != nullptr && element_->HasAccessibilityToggle()) {
            *object = static_cast<IToggleProvider*>(this);
        } else if (iid == __uuidof(IValueProvider) &&
                   element_ != nullptr && element_->HasAccessibilityValue()) {
            *object = static_cast<IValueProvider*>(this);
        } else if (iid == __uuidof(IUnknown)) {
            *object = static_cast<IRawElementProviderSimple*>(this);
        }
        if (*object == nullptr) {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&references_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const LONG remaining = InterlockedDecrement(&references_);
        if (remaining == 0) {
            if (root_ != nullptr) {
                root_->Release();
            }
            delete this;
        }
        return static_cast<ULONG>(remaining);
    }

    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* options) override {
        if (options == nullptr) {
            return E_INVALIDARG;
        }
        *options = ProviderOptions_ServerSideProvider;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID pattern, IUnknown** provider) override {
        if (provider == nullptr) {
            return E_INVALIDARG;
        }
        *provider = nullptr;
        if (element_ == nullptr) {
            return S_OK;
        }
        if (pattern == UIA_InvokePatternId && element_->HasAccessibilityInvoke()) {
            *provider = static_cast<IUnknown*>(static_cast<IInvokeProvider*>(this));
        } else if (pattern == UIA_TogglePatternId && element_->HasAccessibilityToggle()) {
            *provider = static_cast<IUnknown*>(static_cast<IToggleProvider*>(this));
        } else if (pattern == UIA_ValuePatternId && element_->HasAccessibilityValue()) {
            *provider = static_cast<IUnknown*>(static_cast<IValueProvider*>(this));
        }
        if (*provider != nullptr) {
            (*provider)->AddRef();
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID property, VARIANT* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        VariantInit(value);
        if (element_ == nullptr) {
            return S_OK;
        }
        switch (property) {
            case UIA_NamePropertyId:
                VariantString(value, element_->AccessibilityName());
                break;
            case UIA_ControlTypePropertyId:
                VariantInt(value, static_cast<int>(ControlTypeForRole(element_->AccessibilityRole())));
                break;
            case UIA_ValueValuePropertyId:
                if (element_->HasAccessibilityValue()) {
                    VariantString(value, element_->AccessibilityValue());
                }
                break;
            case UIA_ToggleToggleStatePropertyId:
                if (element_->HasAccessibilityToggle()) {
                    VariantInt(
                        value,
                        element_->AccessibilityToggleOn()
                            ? ToggleState_On
                            : ToggleState_Off);
                }
                break;
            case UIA_IsInvokePatternAvailablePropertyId:
                VariantBool(value, element_->HasAccessibilityInvoke());
                break;
            case UIA_IsTogglePatternAvailablePropertyId:
                VariantBool(value, element_->HasAccessibilityToggle());
                break;
            case UIA_IsValuePatternAvailablePropertyId:
                VariantBool(value, element_->HasAccessibilityValue());
                break;
            case UIA_IsEnabledPropertyId:
                VariantBool(value, element_->IsEnabled());
                break;
            case UIA_IsKeyboardFocusablePropertyId:
                VariantBool(value, element_->IsFocusable());
                break;
            case UIA_HasKeyboardFocusPropertyId:
                VariantBool(
                    value,
                    window_ != nullptr && window_->FocusedElement() == element_);
                break;
            case UIA_IsControlElementPropertyId:
            case UIA_IsContentElementPropertyId:
                VariantBool(value, true);
                break;
            case UIA_IsOffscreenPropertyId:
                VariantBool(value, element_->Bounds().IsEmpty());
                break;
            case UIA_AutomationIdPropertyId:
                VariantString(
                    value,
                    L"NopeUI." + std::to_wstring(RuntimeIdFor(element_)));
                break;
            case UIA_ClassNamePropertyId:
                VariantString(value, L"NopeUI.Control");
                break;
            case UIA_FrameworkIdPropertyId:
                VariantString(value, L"NopeUI");
                break;
            case UIA_ProcessIdPropertyId:
                VariantInt(value, static_cast<int>(GetCurrentProcessId()));
                break;
            case UIA_BoundingRectanglePropertyId: {
                UiaRect rect{};
                if (ElementScreenRect(window_, element_, &rect)) {
                    VariantRect(value, rect);
                }
                break;
            }
            default:
                break;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
        IRawElementProviderSimple** provider) override {
        if (provider == nullptr) {
            return E_INVALIDARG;
        }
        *provider = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Navigate(
        NavigateDirection direction,
        IRawElementProviderFragment** provider) override {
        if (provider == nullptr) {
            return E_INVALIDARG;
        }
        *provider = nullptr;
        if (window_ == nullptr || element_ == nullptr) {
            return S_OK;
        }

        Element* parent = element_->Parent();
        if (parent != nullptr) {
            std::vector<Element*> siblings;
            parent->CollectChildren(siblings);
            auto current = std::find_if(
                siblings.begin(),
                siblings.end(),
                [this](Element* candidate) { return candidate == element_; });
            if (current != siblings.end()) {
                if (direction == NavigateDirection_NextSibling) {
                    ++current;
                    if (current != siblings.end()) {
                        *provider = MakeElementProvider(window_, *current, root_);
                    }
                } else if (direction == NavigateDirection_PreviousSibling &&
                           current != siblings.begin()) {
                    --current;
                    *provider = MakeElementProvider(window_, *current, root_);
                }
            }
        } else if (direction == NavigateDirection_PreviousSibling ||
                   direction == NavigateDirection_NextSibling) {
            return S_OK;
        }

        if (direction == NavigateDirection_Parent) {
            if (parent != nullptr) {
                *provider = MakeElementProvider(window_, parent, root_);
            } else if (root_ != nullptr) {
                root_->QueryInterface(
                    __uuidof(IRawElementProviderFragment),
                    reinterpret_cast<void**>(provider));
            }
            return S_OK;
        }

        if (direction == NavigateDirection_FirstChild ||
            direction == NavigateDirection_LastChild) {
            std::vector<Element*> children;
            element_->CollectChildren(children);
            if (!children.empty()) {
                Element* child = direction == NavigateDirection_FirstChild
                    ? children.front()
                    : children.back();
                *provider = MakeElementProvider(window_, child, root_);
            }
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** runtime_id) override {
        if (runtime_id == nullptr) {
            return E_INVALIDARG;
        }
        *runtime_id = CreateRuntimeId(RuntimeIdFor(element_));
        return *runtime_id != nullptr ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* rect) override {
        if (rect == nullptr) {
            return E_INVALIDARG;
        }
        UiaRect bounds{};
        ElementScreenRect(window_, element_, &bounds);
        *rect = bounds;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** roots) override {
        if (roots == nullptr) {
            return E_INVALIDARG;
        }
        *roots = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetFocus() override {
        if (window_ != nullptr && element_ != nullptr && element_->IsFocusable()) {
            window_->FocusFromAccessibility(element_);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_FragmentRoot(
        IRawElementProviderFragmentRoot** root) override {
        if (root == nullptr) {
            return E_INVALIDARG;
        }
        *root = root_;
        if (root_ != nullptr) {
            root_->AddRef();
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke() override {
        if (element_ == nullptr || !element_->HasAccessibilityInvoke()) {
            return UIA_E_NOTSUPPORTED;
        }
        if (!element_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        element_->InvokeAccessibility();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* state) override {
        if (state == nullptr) {
            return E_INVALIDARG;
        }
        if (element_ == nullptr || !element_->HasAccessibilityToggle()) {
            *state = ToggleState_Indeterminate;
        } else {
            *state = element_->AccessibilityToggleOn()
                ? ToggleState_On
                : ToggleState_Off;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Toggle() override {
        if (element_ == nullptr || !element_->HasAccessibilityToggle()) {
            return UIA_E_NOTSUPPORTED;
        }
        if (!element_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        element_->ToggleAccessibility();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR value) override {
        if (element_ == nullptr || value == nullptr) {
            return E_INVALIDARG;
        }
        if (!element_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        return element_->SetAccessibilityValue(value) ? S_OK : E_FAIL;
    }

    HRESULT STDMETHODCALLTYPE get_Value(BSTR* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        const std::wstring text = element_ != nullptr
            ? element_->AccessibilityValue()
            : std::wstring{};
        *value = SysAllocStringLen(
            text.c_str(),
            static_cast<UINT>(text.size()));
        return *value != nullptr ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* read_only) override {
        if (read_only == nullptr) {
            return E_INVALIDARG;
        }
        *read_only = FALSE;
        return S_OK;
    }

    LONG references_ = 1;
    Window* window_ = nullptr;
    Element* element_ = nullptr;
    IRawElementProviderFragmentRoot* root_ = nullptr;
};

IRawElementProviderFragment* MakeElementProvider(
    Window* window,
    Element* element,
    IRawElementProviderFragmentRoot* root) {
    return static_cast<IRawElementProviderFragment*>(
        new ElementProvider(window, element, root));
}

std::unordered_map<Window*, WindowProvider*>& WindowProviders() {
    static std::unordered_map<Window*, WindowProvider*> providers;
    return providers;
}

WindowProvider* EnsureWindowProvider(Window* window) {
    auto& providers = WindowProviders();
    auto found = providers.find(window);
    if (found != providers.end()) {
        return found->second;
    }
    WindowProvider* provider = new WindowProvider(window);
    providers.emplace(window, provider);
    return provider;
}

}  // namespace

bool HandleAutomationGetObject(
    Window* window,
    HWND hwnd,
    WPARAM wparam,
    LPARAM lparam,
    LRESULT* result) noexcept {
    if (window == nullptr || result == nullptr || lparam != OBJID_CLIENT) {
        return false;
    }
    WindowProvider* provider = EnsureWindowProvider(window);
    *result = UiaReturnRawElementProvider(
        hwnd,
        wparam,
        lparam,
        static_cast<IRawElementProviderSimple*>(provider));
    return true;
}

void ReleaseAutomation(Window* window) noexcept {
    auto& providers = WindowProviders();
    auto found = providers.find(window);
    if (found == providers.end()) {
        return;
    }
    found->second->Detach();
    found->second->Release();
    providers.erase(found);
}

void NotifyAutomationFocus(Window* window, Element* element) noexcept {
    if (window == nullptr || element == nullptr) {
        return;
    }
    auto& providers = WindowProviders();
    auto found = providers.find(window);
    if (found == providers.end()) {
        return;
    }
    WindowProvider* root = found->second;
    ElementProvider* provider = new ElementProvider(window, element, root);
    UiaRaiseAutomationEvent(
        static_cast<IRawElementProviderSimple*>(provider),
        UIA_AutomationFocusChangedEventId);
    provider->Release();
}

}  // namespace nopeui
