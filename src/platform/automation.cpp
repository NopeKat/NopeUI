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
#include <cwctype>
#include <mutex>
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
        case AccessibleRole::RadioButton:
            return UIA_RadioButtonControlTypeId;
        case AccessibleRole::Edit:
            return UIA_EditControlTypeId;
        case AccessibleRole::Slider:
            return UIA_SliderControlTypeId;
        case AccessibleRole::ProgressBar:
            return UIA_ProgressBarControlTypeId;
        case AccessibleRole::Group:
            return UIA_GroupControlTypeId;
        case AccessibleRole::Window:
            return UIA_WindowControlTypeId;
        case AccessibleRole::MenuBar:
            return UIA_MenuBarControlTypeId;
        case AccessibleRole::Menu:
            return UIA_MenuControlTypeId;
        case AccessibleRole::MenuItem:
            return UIA_MenuItemControlTypeId;
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

void VariantDouble(VARIANT* value, double number) {
    VariantInit(value);
    value->vt = VT_R8;
    value->dblVal = number;
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

bool ElementLocalScreenRect(
    Window* window,
    Element* element,
    const Rect& local,
    UiaRect* rect) {
    if (local.IsEmpty() || rect == nullptr) {
        return false;
    }
    UiaRect origin{};
    if (!ElementScreenRect(window, element, &origin)) {
        return false;
    }
    rect->left = origin.left + local.x;
    rect->top = origin.top + local.y;
    rect->width = local.width;
    rect->height = local.height;
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

struct TrackedAutomationProvider {
    IRawElementProviderSimple* provider = nullptr;
    Window* window = nullptr;
};

std::mutex& AutomationProviderMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<TrackedAutomationProvider>& TrackedAutomationProviders() {
    static std::vector<TrackedAutomationProvider> providers;
    return providers;
}

void TrackAutomationProvider(IRawElementProviderSimple* provider, Window* window) {
    if (provider == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(AutomationProviderMutex());
    TrackedAutomationProviders().push_back(TrackedAutomationProvider{provider, window});
}

void UntrackAutomationProvider(const IRawElementProviderSimple* provider) {
    std::lock_guard<std::mutex> lock(AutomationProviderMutex());
    auto& providers = TrackedAutomationProviders();
    providers.erase(
        std::remove_if(
            providers.begin(),
            providers.end(),
            [provider](const TrackedAutomationProvider& entry) {
                return entry.provider == provider;
            }),
        providers.end());
}

void DisconnectAutomationProviders(Window* window) {
    std::vector<IRawElementProviderSimple*> providers;
    {
        std::lock_guard<std::mutex> lock(AutomationProviderMutex());
        for (const auto& entry : TrackedAutomationProviders()) {
            if (entry.window == window) {
                providers.push_back(entry.provider);
            }
        }
        TrackedAutomationProviders().erase(
            std::remove_if(
                TrackedAutomationProviders().begin(),
                TrackedAutomationProviders().end(),
                [window](const TrackedAutomationProvider& entry) {
                    return entry.window == window;
                }),
            TrackedAutomationProviders().end());
    }
    for (IRawElementProviderSimple* provider : providers) {
        // UIA marshals to this thread, so no callback is running now; this
        // invalidates any client-held reference before the window dies.
        UiaDisconnectProvider(provider);
    }
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

IRawElementProviderFragment* MakeMenuItemProvider(
    Window* window,
    Element* owner,
    std::size_t index,
    std::wstring name,
    IRawElementProviderFragmentRoot* root);

bool IsWordCharacter(wchar_t character) noexcept {
    return character != L' ' && character != L'\t' &&
           character != L'\n' && character != L'\r';
}

int NextWordStart(const std::wstring& text, int position) noexcept {
    const int length = static_cast<int>(text.size());
    int index = std::clamp(position, 0, length);
    if (index < length && IsWordCharacter(text[static_cast<std::size_t>(index)])) {
        while (index < length &&
               IsWordCharacter(text[static_cast<std::size_t>(index)])) {
            ++index;
        }
    }
    while (index < length &&
           !IsWordCharacter(text[static_cast<std::size_t>(index)])) {
        ++index;
    }
    return index;
}

int PreviousWordStart(const std::wstring& text, int position) noexcept {
    int index = std::clamp(position, 0, static_cast<int>(text.size()));
    while (index > 0 && !IsWordCharacter(text[static_cast<std::size_t>(index - 1)])) {
        --index;
    }
    while (index > 0 && IsWordCharacter(text[static_cast<std::size_t>(index - 1)])) {
        --index;
    }
    return index;
}

int MoveTextOffset(
    const std::wstring& text,
    int position,
    TextUnit unit,
    int count) noexcept {
    const int length = static_cast<int>(text.size());
    int index = std::clamp(position, 0, length);
    if (unit == TextUnit_Character) {
        index = std::clamp(index + count, 0, length);
        return index - position;
    }
    if (unit != TextUnit_Word) {
        return 0;
    }
    int moved = 0;
    if (count > 0) {
        for (int step = 0; step < count; ++step) {
            const int next = NextWordStart(text, index);
            if (next == index) {
                break;
            }
            index = next;
            ++moved;
        }
    } else if (count < 0) {
        for (int step = 0; step < -count; ++step) {
            const int previous = PreviousWordStart(text, index);
            if (previous == index) {
                break;
            }
            index = previous;
            --moved;
        }
    }
    return moved;
}

class TextRangeProvider final : public ITextRangeProvider {
public:
    TextRangeProvider(Window* window, Element* element, int start, int end) noexcept
        : window_(window), element_(element), start_(start), end_(end) {
        Clamp();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (object == nullptr) {
            return E_INVALIDARG;
        }
        *object = nullptr;
        if (iid == __uuidof(ITextRangeProvider) || iid == __uuidof(IUnknown)) {
            *object = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
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

    HRESULT STDMETHODCALLTYPE Clone(ITextRangeProvider** result) override {
        if (result == nullptr) {
            return E_INVALIDARG;
        }
        *result = new TextRangeProvider(window_, element_, start_, end_);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Compare(ITextRangeProvider* range, BOOL* equal) override {
        if (range == nullptr || equal == nullptr) {
            return E_INVALIDARG;
        }
        const TextRangeProvider* other = dynamic_cast<TextRangeProvider*>(range);
        if (other == nullptr) {
            return E_INVALIDARG;
        }
        *equal = (other->element_ == element_ &&
                  other->start_ == start_ &&
                  other->end_ == end_)
            ? TRUE
            : FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CompareEndpoints(
        TextPatternRangeEndpoint endpoint,
        ITextRangeProvider* target_range,
        TextPatternRangeEndpoint target_endpoint,
        int* result) override {
        if (target_range == nullptr || result == nullptr) {
            return E_INVALIDARG;
        }
        const TextRangeProvider* other = dynamic_cast<TextRangeProvider*>(target_range);
        if (other == nullptr) {
            return E_INVALIDARG;
        }
        const int value = endpoint == TextPatternRangeEndpoint_Start ? start_ : end_;
        const int target = target_endpoint == TextPatternRangeEndpoint_Start
            ? other->start_
            : other->end_;
        *result = value < target ? -1 : (value > target ? 1 : 0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ExpandToEnclosingUnit(TextUnit unit) override {
        const std::wstring text = CurrentText();
        const int length = static_cast<int>(text.size());
        switch (unit) {
            case TextUnit_Character:
                if (start_ == end_ && start_ < length) {
                    end_ = start_ + 1;
                }
                break;
            case TextUnit_Word:
                if (start_ == end_ && start_ < length &&
                    IsWordCharacter(text[static_cast<std::size_t>(start_)])) {
                    while (start_ > 0 &&
                           IsWordCharacter(text[static_cast<std::size_t>(start_ - 1)])) {
                        --start_;
                    }
                    while (end_ < length &&
                           IsWordCharacter(text[static_cast<std::size_t>(end_)])) {
                        ++end_;
                    }
                }
                break;
            case TextUnit_Line:
            case TextUnit_Paragraph:
            case TextUnit_Page:
            case TextUnit_Document:
            case TextUnit_Format:
                start_ = 0;
                end_ = length;
                break;
            default:
                break;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FindAttribute(
        TEXTATTRIBUTEID,
        VARIANT,
        BOOL,
        ITextRangeProvider** result) override {
        if (result != nullptr) {
            *result = nullptr;
        }
        return S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE FindText(
        BSTR text,
        BOOL backward,
        BOOL ignore_case,
        ITextRangeProvider** result) override {
        if (result == nullptr) {
            return E_INVALIDARG;
        }
        *result = nullptr;
        if (text == nullptr || text[0] == L'\0') {
            return S_FALSE;
        }
        std::wstring needle(text, SysStringLen(text));
        std::wstring haystack = CurrentText();
        if (ignore_case != FALSE) {
            needle = ToLower(needle);
            haystack = ToLower(haystack);
        }
        int found = -1;
        const int needle_length = static_cast<int>(needle.size());
        if (backward == FALSE) {
            for (int index = start_; index + needle_length <= end_; ++index) {
                if (haystack.compare(
                        static_cast<std::size_t>(index),
                        static_cast<std::size_t>(needle_length),
                        needle) == 0) {
                    found = index;
                    break;
                }
            }
        } else {
            for (int index = end_ - needle_length; index >= start_; --index) {
                if (haystack.compare(
                        static_cast<std::size_t>(index),
                        static_cast<std::size_t>(needle_length),
                        needle) == 0) {
                    found = index;
                    break;
                }
            }
        }
        if (found < 0) {
            return S_FALSE;
        }
        *result = new TextRangeProvider(
            window_,
            element_,
            found,
            found + needle_length);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetAttributeValue(
        TEXTATTRIBUTEID,
        VARIANT* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        VariantInit(value);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetBoundingRectangles(SAFEARRAY** rectangles) override {
        if (rectangles == nullptr) {
            return E_INVALIDARG;
        }
        *rectangles = SafeArrayCreateVector(VT_R8, 0, 4);
        if (*rectangles == nullptr) {
            return E_OUTOFMEMORY;
        }
        UiaRect bounds{};
        if (!ElementScreenRect(window_, element_, &bounds)) {
            return S_OK;
        }
        double values[4] = {bounds.left, bounds.top, bounds.width, bounds.height};
        for (LONG index = 0; index < 4; ++index) {
            SafeArrayPutElement(*rectangles, &index, &values[index]);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEnclosingElement(
        IRawElementProviderSimple** result) override {
        if (result == nullptr) {
            return E_INVALIDARG;
        }
        *result = nullptr;
        IRawElementProviderFragment* fragment = MakeElementProvider(
            window_,
            element_,
            nullptr);
        if (fragment == nullptr) {
            return S_OK;
        }
        fragment->QueryInterface(
            __uuidof(IRawElementProviderSimple),
            reinterpret_cast<void**>(result));
        fragment->Release();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetText(int max_length, BSTR* result) override {
        if (result == nullptr) {
            return E_INVALIDARG;
        }
        std::wstring text = CurrentText();
        const int length = end_ - start_;
        int count = length;
        if (max_length >= 0 && max_length < count) {
            count = max_length;
        }
        const std::wstring slice = text.substr(
            static_cast<std::size_t>(start_),
            static_cast<std::size_t>(count));
        *result = SysAllocStringLen(
            slice.c_str(),
            static_cast<UINT>(slice.size()));
        return *result != nullptr || slice.empty() ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE Move(TextUnit unit, int count, int* moved) override {
        if (moved == nullptr) {
            return E_INVALIDARG;
        }
        const std::wstring text = CurrentText();
        const int delta = MoveTextOffset(text, start_, unit, count);
        if (delta != 0) {
            start_ += delta;
            end_ += delta;
            Clamp();
        }
        *moved = delta;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE MoveEndpointByUnit(
        TextPatternRangeEndpoint endpoint,
        TextUnit unit,
        int count,
        int* moved) override {
        if (moved == nullptr) {
            return E_INVALIDARG;
        }
        const std::wstring text = CurrentText();
        int* position = endpoint == TextPatternRangeEndpoint_Start ? &start_ : &end_;
        const int delta = MoveTextOffset(text, *position, unit, count);
        *position += delta;
        if (start_ > end_) {
            start_ = end_;
        }
        Clamp();
        *moved = delta;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE MoveEndpointByRange(
        TextPatternRangeEndpoint endpoint,
        ITextRangeProvider* target_range,
        TextPatternRangeEndpoint target_endpoint) override {
        if (target_range == nullptr) {
            return E_INVALIDARG;
        }
        const TextRangeProvider* other = dynamic_cast<TextRangeProvider*>(target_range);
        if (other == nullptr) {
            return E_INVALIDARG;
        }
        if (endpoint == TextPatternRangeEndpoint_Start) {
            start_ = target_endpoint == TextPatternRangeEndpoint_Start
                ? other->start_
                : other->end_;
            if (start_ > end_) {
                start_ = end_;
            }
        } else {
            end_ = target_endpoint == TextPatternRangeEndpoint_Start
                ? other->start_
                : other->end_;
            if (end_ < start_) {
                end_ = start_;
            }
        }
        Clamp();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Select() override {
        if (element_ == nullptr || !element_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        return element_->SetAccessibilitySelection(start_, end_)
            ? S_OK
            : UIA_E_NOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE AddToSelection() override {
        return UIA_E_NOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE RemoveFromSelection() override {
        return UIA_E_NOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE ScrollIntoView(BOOL) override {
        if (element_ != nullptr && element_->Parent() != nullptr) {
            element_->Parent()->BringChildIntoView(element_);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetChildren(SAFEARRAY** children) override {
        if (children == nullptr) {
            return E_INVALIDARG;
        }
        *children = nullptr;
        return S_OK;
    }

    int Start() const noexcept {
        return start_;
    }

    int End() const noexcept {
        return end_;
    }

private:
    void Clamp() noexcept {
        const int length = element_ != nullptr
            ? static_cast<int>(element_->AccessibilityText().size())
            : 0;
        start_ = std::clamp(start_, 0, length);
        end_ = std::clamp(end_, 0, length);
        if (start_ > end_) {
            start_ = end_;
        }
    }

    std::wstring CurrentText() const {
        return element_ != nullptr ? element_->AccessibilityText() : std::wstring{};
    }

    static std::wstring ToLower(std::wstring text) {
        for (wchar_t& character : text) {
            character = static_cast<wchar_t>(towlower(character));
        }
        return text;
    }

    LONG references_ = 1;
    Window* window_ = nullptr;
    Element* element_ = nullptr;
    int start_ = 0;
    int end_ = 0;
};

class WindowProvider final : public IRawElementProviderSimple,
                             public IRawElementProviderFragment,
                             public IRawElementProviderFragmentRoot {
public:
    explicit WindowProvider(Window* window) noexcept : window_(window) {
        TrackAutomationProvider(static_cast<IRawElementProviderSimple*>(this), window);
    }

    ~WindowProvider() {
        UntrackAutomationProvider(static_cast<IRawElementProviderSimple*>(this));
    }

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
                              public IValueProvider,
                              public IRangeValueProvider,
                              public IScrollProvider,
                              public ITextProvider {
public:
    ElementProvider(
        Window* window,
        Element* element,
        IRawElementProviderFragmentRoot* root) noexcept
        : window_(window), element_(element), root_(root) {
        if (root_ != nullptr) {
            root_->AddRef();
        }
        TrackAutomationProvider(static_cast<IRawElementProviderSimple*>(this), window);
    }

    ~ElementProvider() {
        UntrackAutomationProvider(static_cast<IRawElementProviderSimple*>(this));
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
        } else if (iid == __uuidof(IRangeValueProvider) &&
                   element_ != nullptr && element_->HasAccessibilityRange()) {
            *object = static_cast<IRangeValueProvider*>(this);
        } else if (iid == __uuidof(IScrollProvider) &&
                   element_ != nullptr && element_->HasAccessibilityScroll()) {
            *object = static_cast<IScrollProvider*>(this);
        } else if (iid == __uuidof(ITextProvider) &&
                   element_ != nullptr && element_->HasAccessibilityText()) {
            *object = static_cast<ITextProvider*>(this);
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
        } else if (pattern == UIA_RangeValuePatternId && element_->HasAccessibilityRange()) {
            *provider = static_cast<IUnknown*>(static_cast<IRangeValueProvider*>(this));
        } else if (pattern == UIA_ScrollPatternId && element_->HasAccessibilityScroll()) {
            *provider = static_cast<IUnknown*>(static_cast<IScrollProvider*>(this));
        } else if (pattern == UIA_TextPatternId && element_->HasAccessibilityText()) {
            *provider = static_cast<IUnknown*>(static_cast<ITextProvider*>(this));
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
            case UIA_IsRangeValuePatternAvailablePropertyId:
                VariantBool(value, element_->HasAccessibilityRange());
                break;
            case UIA_IsScrollPatternAvailablePropertyId:
                VariantBool(value, element_->HasAccessibilityScroll());
                break;
            case UIA_IsTextPatternAvailablePropertyId:
                VariantBool(value, element_->HasAccessibilityText());
                break;
            case UIA_RangeValueValuePropertyId:
                if (element_->HasAccessibilityRange()) {
                    VariantDouble(value, element_->AccessibilityRangeValue());
                }
                break;
            case UIA_RangeValueMinimumPropertyId:
                if (element_->HasAccessibilityRange()) {
                    VariantDouble(value, element_->AccessibilityRangeMinimum());
                }
                break;
            case UIA_RangeValueMaximumPropertyId:
                if (element_->HasAccessibilityRange()) {
                    VariantDouble(value, element_->AccessibilityRangeMaximum());
                }
                break;
            case UIA_RangeValueSmallChangePropertyId:
                if (element_->HasAccessibilityRange()) {
                    VariantDouble(value, element_->AccessibilityRangeSmallChange());
                }
                break;
            case UIA_RangeValueLargeChangePropertyId:
                if (element_->HasAccessibilityRange()) {
                    VariantDouble(value, element_->AccessibilityRangeLargeChange());
                }
                break;
            case UIA_RangeValueIsReadOnlyPropertyId:
                if (element_->HasAccessibilityRange()) {
                    VariantBool(value, element_->IsAccessibilityRangeReadOnly());
                }
                break;
            case UIA_ScrollHorizontalScrollPercentPropertyId:
                if (element_->HasAccessibilityScroll()) {
                    VariantDouble(value, element_->AccessibilityHorizontalScrollPercent());
                }
                break;
            case UIA_ScrollVerticalScrollPercentPropertyId:
                if (element_->HasAccessibilityScroll()) {
                    VariantDouble(value, element_->AccessibilityVerticalScrollPercent());
                }
                break;
            case UIA_ScrollHorizontalViewSizePropertyId:
                if (element_->HasAccessibilityScroll()) {
                    VariantDouble(value, element_->AccessibilityHorizontalViewSize());
                }
                break;
            case UIA_ScrollVerticalViewSizePropertyId:
                if (element_->HasAccessibilityScroll()) {
                    VariantDouble(value, element_->AccessibilityVerticalViewSize());
                }
                break;
            case UIA_ScrollHorizontallyScrollablePropertyId:
                if (element_->HasAccessibilityScroll()) {
                    VariantBool(value, element_->AccessibilityHorizontallyScrollable());
                }
                break;
            case UIA_ScrollVerticallyScrollablePropertyId:
                if (element_->HasAccessibilityScroll()) {
                    VariantBool(value, element_->AccessibilityVerticallyScrollable());
                }
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
            } else {
                std::vector<std::wstring> items;
                element_->CollectAccessibleMenuItems(items);
                if (!items.empty()) {
                    const std::size_t index = direction == NavigateDirection_FirstChild
                        ? 0
                        : items.size() - 1;
                    *provider = MakeMenuItemProvider(
                        window_, element_, index, items[index], root_);
                }
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
        if (element_ != nullptr && element_->HasAccessibilityRange()) {
            *read_only = element_->IsAccessibilityRangeReadOnly() ? TRUE : FALSE;
        } else {
            *read_only = FALSE;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetValue(double value) override {
        if (element_ == nullptr || !element_->HasAccessibilityRange()) {
            return UIA_E_NOTSUPPORTED;
        }
        if (!element_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        if (element_->IsAccessibilityRangeReadOnly()) {
            return UIA_E_NOTSUPPORTED;
        }
        return element_->SetAccessibilityRangeValue(value)
            ? S_OK
            : E_FAIL;
    }

    HRESULT STDMETHODCALLTYPE get_Value(double* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr ? element_->AccessibilityRangeValue() : 0.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_Maximum(double* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr ? element_->AccessibilityRangeMaximum() : 0.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_Minimum(double* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr ? element_->AccessibilityRangeMinimum() : 0.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_LargeChange(double* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr ? element_->AccessibilityRangeLargeChange() : 0.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_SmallChange(double* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr ? element_->AccessibilityRangeSmallChange() : 0.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Scroll(
        ScrollAmount horizontal_amount,
        ScrollAmount vertical_amount) override {
        if (element_ == nullptr || !element_->HasAccessibilityScroll()) {
            return UIA_E_NOTSUPPORTED;
        }
        if (!element_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        const double horizontal = ScrollDelta(horizontal_amount);
        const double vertical = ScrollDelta(vertical_amount);
        if (horizontal != 0.0) {
            element_->ScrollAccessibilityHorizontal(horizontal);
        }
        if (vertical != 0.0) {
            element_->ScrollAccessibilityVertical(vertical);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetScrollPercent(
        double horizontal_percent,
        double vertical_percent) override {
        if (element_ == nullptr || !element_->HasAccessibilityScroll()) {
            return UIA_E_NOTSUPPORTED;
        }
        if (!element_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        element_->SetAccessibilityScrollPercent(
            horizontal_percent,
            vertical_percent);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_HorizontalScrollPercent(double* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr
            ? element_->AccessibilityHorizontalScrollPercent()
            : -1.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_VerticalScrollPercent(double* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr
            ? element_->AccessibilityVerticalScrollPercent()
            : -1.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_HorizontalViewSize(double* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr
            ? element_->AccessibilityHorizontalViewSize()
            : 100.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_VerticalViewSize(double* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr
            ? element_->AccessibilityVerticalViewSize()
            : 100.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_HorizontallyScrollable(BOOL* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr && element_->AccessibilityHorizontallyScrollable()
            ? TRUE
            : FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_VerticallyScrollable(BOOL* value) override {
        if (value == nullptr) {
            return E_INVALIDARG;
        }
        *value = element_ != nullptr && element_->AccessibilityVerticallyScrollable()
            ? TRUE
            : FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetSelection(SAFEARRAY** selection) override {
        if (selection == nullptr) {
            return E_INVALIDARG;
        }
        *selection = MakeRangeArray(SelectionRange());
        return *selection != nullptr ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE GetVisibleRanges(SAFEARRAY** ranges) override {
        if (ranges == nullptr) {
            return E_INVALIDARG;
        }
        *ranges = MakeRangeArray(DocumentRange());
        return *ranges != nullptr ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE RangeFromChild(
        IRawElementProviderSimple*,
        ITextRangeProvider** range) override {
        if (range == nullptr) {
            return E_INVALIDARG;
        }
        *range = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RangeFromPoint(
        UiaPoint,
        ITextRangeProvider** range) override {
        if (range == nullptr) {
            return E_INVALIDARG;
        }
        *range = DocumentRange();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_DocumentRange(ITextRangeProvider** range) override {
        if (range == nullptr) {
            return E_INVALIDARG;
        }
        *range = DocumentRange();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_SupportedTextSelection(
        SupportedTextSelection* selection) override {
        if (selection == nullptr) {
            return E_INVALIDARG;
        }
        *selection = SupportedTextSelection_Single;
        return S_OK;
    }

    static double ScrollDelta(ScrollAmount amount) noexcept {
        switch (amount) {
            case ScrollAmount_SmallIncrement:
                return 5.0;
            case ScrollAmount_LargeIncrement:
                return 20.0;
            case ScrollAmount_SmallDecrement:
                return -5.0;
            case ScrollAmount_LargeDecrement:
                return -20.0;
            case ScrollAmount_NoAmount:
            default:
                return 0.0;
        }
    }

    TextRangeProvider* DocumentRange() const {
        const int length = element_ != nullptr
            ? static_cast<int>(element_->AccessibilityText().size())
            : 0;
        return new TextRangeProvider(window_, element_, 0, length);
    }

    TextRangeProvider* SelectionRange() const {
        return new TextRangeProvider(
            window_,
            element_,
            element_ != nullptr ? element_->AccessibilitySelectionStart() : 0,
            element_ != nullptr ? element_->AccessibilitySelectionEnd() : 0);
    }

    static SAFEARRAY* MakeRangeArray(TextRangeProvider* range) {
        SAFEARRAY* array = SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
        if (array == nullptr) {
            range->Release();
            return nullptr;
        }
        LONG index = 0;
        const HRESULT result = SafeArrayPutElement(
            array,
            &index,
            reinterpret_cast<void*>(range));
        range->Release();
        if (FAILED(result)) {
            SafeArrayDestroy(array);
            return nullptr;
        }
        return array;
    }

    LONG references_ = 1;
    Window* window_ = nullptr;
    Element* element_ = nullptr;
    IRawElementProviderFragmentRoot* root_ = nullptr;
};

// Menu rows are painted text rather than elements, so each row gets a small
// provider that reports its own rectangle and forwards invoke or expand
// actions back to the owning control.
class MenuItemProvider final : public IRawElementProviderSimple,
                               public IRawElementProviderFragment,
                               public IInvokeProvider,
                               public IExpandCollapseProvider {
public:
    MenuItemProvider(
        Window* window,
        Element* owner,
        std::size_t index,
        std::wstring name,
        IRawElementProviderFragmentRoot* root) noexcept
        : window_(window), owner_(owner), index_(index),
          name_(std::move(name)), root_(root) {
        TrackAutomationProvider(static_cast<IRawElementProviderSimple*>(this), window);
    }

    ~MenuItemProvider() {
        UntrackAutomationProvider(static_cast<IRawElementProviderSimple*>(this));
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (object == nullptr) {
            return E_INVALIDARG;
        }
        *object = nullptr;
        if (iid == __uuidof(IRawElementProviderSimple) || iid == __uuidof(IUnknown)) {
            *object = static_cast<IRawElementProviderSimple*>(this);
        } else if (iid == __uuidof(IRawElementProviderFragment)) {
            *object = static_cast<IRawElementProviderFragment*>(this);
        } else if (iid == __uuidof(IInvokeProvider)) {
            *object = static_cast<IInvokeProvider*>(this);
        } else if (iid == __uuidof(IExpandCollapseProvider) &&
                   owner_ != nullptr &&
                   owner_->HasAccessibilityMenuItemExpandCollapse()) {
            *object = static_cast<IExpandCollapseProvider*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --references_;
        if (remaining == 0) {
            delete this;
        }
        return remaining;
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
        if (pattern == UIA_InvokePatternId) {
            *provider = static_cast<IUnknown*>(static_cast<IInvokeProvider*>(this));
        } else if (pattern == UIA_ExpandCollapsePatternId &&
                   owner_ != nullptr &&
                   owner_->HasAccessibilityMenuItemExpandCollapse()) {
            *provider = static_cast<IUnknown*>(
                static_cast<IExpandCollapseProvider*>(this));
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
        switch (property) {
            case UIA_ControlTypePropertyId:
                VariantInt(value, UIA_MenuItemControlTypeId);
                break;
            case UIA_NamePropertyId:
                VariantString(value, name_);
                break;
            case UIA_IsEnabledPropertyId:
                VariantBool(value, owner_ != nullptr && owner_->IsEnabled());
                break;
            case UIA_IsKeyboardFocusablePropertyId:
                VariantBool(value, owner_ != nullptr && owner_->IsFocusable());
                break;
            case UIA_IsControlElementPropertyId:
            case UIA_IsContentElementPropertyId:
                VariantBool(value, true);
                break;
            case UIA_IsInvokePatternAvailablePropertyId:
                VariantBool(value, owner_ != nullptr);
                break;
            case UIA_IsExpandCollapsePatternAvailablePropertyId:
                VariantBool(
                    value,
                    owner_ != nullptr &&
                        owner_->HasAccessibilityMenuItemExpandCollapse());
                break;
            case UIA_ExpandCollapseExpandCollapseStatePropertyId:
                if (owner_ != nullptr &&
                    owner_->HasAccessibilityMenuItemExpandCollapse()) {
                    VariantInt(
                        value,
                        static_cast<int>(
                            owner_->AccessibilityMenuItemExpanded(index_)
                                ? ExpandCollapseState_Expanded
                                : ExpandCollapseState_Collapsed));
                }
                break;
            case UIA_IsOffscreenPropertyId: {
                UiaRect bounds{};
                VariantBool(
                    value,
                    !ElementLocalScreenRect(
                        window_,
                        owner_,
                        owner_ != nullptr
                            ? owner_->AccessibilityMenuItemBounds(index_)
                            : Rect{},
                        &bounds));
                break;
            }
            case UIA_BoundingRectanglePropertyId: {
                UiaRect rect{};
                if (owner_ != nullptr &&
                    ElementLocalScreenRect(
                        window_,
                        owner_,
                        owner_->AccessibilityMenuItemBounds(index_),
                        &rect)) {
                    VariantRect(value, rect);
                }
                break;
            }
            case UIA_ClassNamePropertyId:
                VariantString(value, L"NopeUI.MenuItem");
                break;
            case UIA_FrameworkIdPropertyId:
                VariantString(value, L"NopeUI");
                break;
            case UIA_ProcessIdPropertyId:
                VariantInt(value, static_cast<int>(GetCurrentProcessId()));
                break;
            case UIA_AutomationIdPropertyId:
                if (owner_ != nullptr) {
                    VariantString(
                        value,
                        L"NopeUI.MenuItem." +
                            std::to_wstring(RuntimeIdFor(owner_)) + L"." +
                            std::to_wstring(index_));
                }
                break;
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
        const std::size_t count = ItemCount();
        if (direction == NavigateDirection_Parent) {
            if (owner_ != nullptr) {
                *provider = MakeElementProvider(window_, owner_, root_);
            }
            return S_OK;
        }
        if (count == 0) {
            return S_OK;
        }
        if (direction == NavigateDirection_NextSibling && index_ + 1 < count) {
            *provider = MakeMenuItemProvider(
                window_, owner_, index_ + 1, ItemName(index_ + 1), root_);
        } else if (direction == NavigateDirection_PreviousSibling && index_ > 0) {
            *provider = MakeMenuItemProvider(
                window_, owner_, index_ - 1, ItemName(index_ - 1), root_);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** runtime_id) override {
        if (runtime_id == nullptr) {
            return E_INVALIDARG;
        }
        const int identifier =
            (RuntimeIdFor(owner_) ^ ((static_cast<int>(index_) + 1) * 7919)) & 0x7fffffff;
        *runtime_id = CreateRuntimeId(identifier);
        return *runtime_id != nullptr ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* rect) override {
        if (rect == nullptr) {
            return E_INVALIDARG;
        }
        UiaRect bounds{};
        if (owner_ != nullptr) {
            ElementLocalScreenRect(
                window_,
                owner_,
                owner_->AccessibilityMenuItemBounds(index_),
                &bounds);
        }
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
        if (window_ != nullptr && owner_ != nullptr && owner_->IsFocusable()) {
            window_->FocusFromAccessibility(owner_);
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
        if (owner_ == nullptr) {
            return UIA_E_NOTSUPPORTED;
        }
        if (!owner_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        return owner_->InvokeAccessibilityMenuItem(index_)
            ? S_OK
            : UIA_E_NOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(
        ExpandCollapseState* state) override {
        if (state == nullptr) {
            return E_INVALIDARG;
        }
        if (owner_ == nullptr ||
            !owner_->HasAccessibilityMenuItemExpandCollapse()) {
            return UIA_E_NOTSUPPORTED;
        }
        *state = owner_->AccessibilityMenuItemExpanded(index_)
            ? ExpandCollapseState_Expanded
            : ExpandCollapseState_Collapsed;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Expand() override {
        if (owner_ == nullptr ||
            !owner_->HasAccessibilityMenuItemExpandCollapse()) {
            return UIA_E_NOTSUPPORTED;
        }
        if (!owner_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        return owner_->SetAccessibilityMenuItemExpanded(index_, true)
            ? S_OK
            : UIA_E_NOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE Collapse() override {
        if (owner_ == nullptr ||
            !owner_->HasAccessibilityMenuItemExpandCollapse()) {
            return UIA_E_NOTSUPPORTED;
        }
        if (!owner_->IsEnabled()) {
            return UIA_E_ELEMENTNOTENABLED;
        }
        return owner_->SetAccessibilityMenuItemExpanded(index_, false)
            ? S_OK
            : UIA_E_NOTSUPPORTED;
    }

private:
    std::size_t ItemCount() const {
        std::vector<std::wstring> items;
        if (owner_ != nullptr) {
            owner_->CollectAccessibleMenuItems(items);
        }
        return items.size();
    }

    std::wstring ItemName(std::size_t index) const {
        std::vector<std::wstring> items;
        if (owner_ != nullptr) {
            owner_->CollectAccessibleMenuItems(items);
        }
        return index < items.size() ? items[index] : std::wstring();
    }

    LONG references_ = 1;
    Window* window_ = nullptr;
    Element* owner_ = nullptr;
    std::size_t index_ = 0;
    std::wstring name_;
    IRawElementProviderFragmentRoot* root_ = nullptr;
};

IRawElementProviderFragment* MakeElementProvider(
    Window* window,
    Element* element,
    IRawElementProviderFragmentRoot* root) {
    return static_cast<IRawElementProviderFragment*>(
        new ElementProvider(window, element, root));
}

IRawElementProviderFragment* MakeMenuItemProvider(
    Window* window,
    Element* owner,
    std::size_t index,
    std::wstring name,
    IRawElementProviderFragmentRoot* root) {
    return static_cast<IRawElementProviderFragment*>(
        new MenuItemProvider(window, owner, index, std::move(name), root));
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
    if (window == nullptr || result == nullptr) {
        return false;
    }
    // UI Automation asks for the root provider with UiaRootObjectId. MSAA's
    // OBJID_CLIENT request is left to DefWindowProc because this window does
    // not expose an IAccessible object.
    if (static_cast<LONG>(lparam) != UiaRootObjectId) {
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
    DisconnectAutomationProviders(window);
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
