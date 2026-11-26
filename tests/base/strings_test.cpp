#include "../test_check.h"
#include <string>

#include "nopeui/base/strings.h"

int main() {
    const auto ascii = nopeui::Utf8ToUtf16("Hello");
    NOPEUI_CHECK(ascii.ok());
    NOPEUI_CHECK(ascii.value() == std::u16string(u"Hello"));

    const auto chinese = nopeui::Utf8ToUtf16("你好");
    NOPEUI_CHECK(chinese.ok());
    NOPEUI_CHECK(chinese.value() == std::u16string(u"你好"));

    const auto emoji = nopeui::Utf8ToUtf16("😀");
    NOPEUI_CHECK(emoji.ok());
    NOPEUI_CHECK(emoji.value() == std::u16string(u"\U0001F600"));

    const std::u16string mixed(u"你好\U0001F600");
    const auto utf8 = nopeui::Utf16ToUtf8(mixed);
    NOPEUI_CHECK(utf8.ok());

    const auto roundtrip = nopeui::Utf8ToUtf16(utf8.value());
    NOPEUI_CHECK(roundtrip.ok());
    NOPEUI_CHECK(roundtrip.value() == mixed);

    std::string invalid_utf8;
    invalid_utf8.push_back(static_cast<char>(0xFF));
    const auto bad_utf8 = nopeui::Utf8ToUtf16(invalid_utf8);
    NOPEUI_CHECK(!bad_utf8.ok());
    NOPEUI_CHECK(bad_utf8.error().code == nopeui::ErrorCode::InvalidUtf8);

    std::u16string invalid_utf16;
    invalid_utf16.push_back(static_cast<char16_t>(0xD800));
    const auto bad_utf16 = nopeui::Utf16ToUtf8(invalid_utf16);
    NOPEUI_CHECK(!bad_utf16.ok());
    NOPEUI_CHECK(bad_utf16.error().code == nopeui::ErrorCode::InvalidUtf16);

    return 0;
}
