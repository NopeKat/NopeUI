#ifndef NOPEUI_BASE_STRINGS_H_
#define NOPEUI_BASE_STRINGS_H_

#include <string>
#include <string_view>

#include "nopeui/base/result.h"

namespace nopeui {

Result<std::u16string> Utf8ToUtf16(std::string_view text);
Result<std::string> Utf16ToUtf8(std::u16string_view text);

}  // namespace nopeui

#endif  // NOPEUI_BASE_STRINGS_H_
