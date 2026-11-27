#include "../test_check.h"

#include <string>

#include <Windows.h>

#include "nopeui/controls/text_box.h"

namespace {

const std::wstring kSurrogate =
    std::wstring(1, static_cast<wchar_t>(0xD83D)) +
    static_cast<wchar_t>(0xDE00);

}  // namespace

int main() {
    auto text = nopeui::TextBox::Create();
    nopeui::Element* element = text.Get();

    // Backspace at the end removes the whole surrogate pair, not one half.
    text->SetText(kSurrogate);
    NOPEUI_CHECK(text->Text().size() == 2);
    NOPEUI_CHECK(element->OnKeyDown(VK_BACK));
    NOPEUI_CHECK(text->Text().empty());

    // Delete removes a pair at the caret without leaving a lone surrogate.
    text->SetText(kSurrogate + L"B");
    NOPEUI_CHECK(element->SetAccessibilitySelection(0, 0));
    NOPEUI_CHECK(element->OnKeyDown(VK_DELETE));
    NOPEUI_CHECK(text->Text() == L"B");

    // Left and right steps cross the pair as one character.
    text->SetText(L"A" + kSurrogate + L"B");
    NOPEUI_CHECK(element->AccessibilitySelectionStart() == 4);
    NOPEUI_CHECK(element->OnKeyDown(VK_LEFT));
    NOPEUI_CHECK(element->AccessibilitySelectionStart() == 3);
    NOPEUI_CHECK(element->OnKeyDown(VK_LEFT));
    NOPEUI_CHECK(element->AccessibilitySelectionStart() == 1);
    NOPEUI_CHECK(element->OnKeyDown(VK_RIGHT));
    NOPEUI_CHECK(element->AccessibilitySelectionStart() == 3);
    NOPEUI_CHECK(element->OnKeyDown(VK_RIGHT));
    NOPEUI_CHECK(element->AccessibilitySelectionStart() == 4);

    return 0;
}
