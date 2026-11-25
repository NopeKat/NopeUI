#include <cassert>
#include <string>

#include "nopeui/base/strings.h"

int main() {
    const auto ascii = nopeui::Utf8ToUtf16("Hello");
    assert(ascii.ok());
    assert(ascii.value() == std::u16string(u"Hello"));

    const auto chinese = nopeui::Utf8ToUtf16("你好");
    assert(chinese.ok());
    assert(chinese.value() == std::u16string(u"你好"));

    const auto emoji = nopeui::Utf8ToUtf16("😀");
    assert(emoji.ok());
    assert(emoji.value() == std::u16string(u"\U0001F600"));

    const std::u16string mixed(u"你好\U0001F600");
    const auto utf8 = nopeui::Utf16ToUtf8(mixed);
    assert(utf8.ok());

    const auto roundtrip = nopeui::Utf8ToUtf16(utf8.value());
    assert(roundtrip.ok());
    assert(roundtrip.value() == mixed);

    std::string invalid_utf8;
    invalid_utf8.push_back(static_cast<char>(0xFF));
    const auto bad_utf8 = nopeui::Utf8ToUtf16(invalid_utf8);
    assert(!bad_utf8.ok());
    assert(bad_utf8.error().code == nopeui::ErrorCode::InvalidUtf8);

    std::u16string invalid_utf16;
    invalid_utf16.push_back(static_cast<char16_t>(0xD800));
    const auto bad_utf16 = nopeui::Utf16ToUtf8(invalid_utf16);
    assert(!bad_utf16.ok());
    assert(bad_utf16.error().code == nopeui::ErrorCode::InvalidUtf16);

    return 0;
}
