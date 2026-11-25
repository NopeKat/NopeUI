#include "nopeui/base/strings.h"

#include <cstdint>
#include <string>

namespace {

std::uint8_t ByteAt(std::string_view text, std::size_t index) {
    return static_cast<std::uint8_t>(static_cast<unsigned char>(text[index]));
}

bool IsContinuation(std::uint8_t byte) {
    return (byte & 0xC0u) == 0x80u;
}

bool DecodeUtf8(std::string_view text, std::size_t& index, std::uint32_t& code_point) {
    if (index >= text.size()) {
        return false;
    }

    const std::uint8_t first = ByteAt(text, index);
    if (first < 0x80u) {
        code_point = first;
        ++index;
        return true;
    }

    std::size_t length = 0;
    std::uint32_t minimum = 0;

    if ((first & 0xE0u) == 0xC0u) {
        length = 2;
        code_point = static_cast<std::uint32_t>(first & 0x1Fu);
        minimum = 0x80u;
    } else if ((first & 0xF0u) == 0xE0u) {
        length = 3;
        code_point = static_cast<std::uint32_t>(first & 0x0Fu);
        minimum = 0x800u;
    } else if ((first & 0xF8u) == 0xF0u) {
        length = 4;
        code_point = static_cast<std::uint32_t>(first & 0x07u);
        minimum = 0x10000u;
    } else {
        return false;
    }

    if (text.size() - index < length) {
        return false;
    }

    for (std::size_t offset = 1; offset < length; ++offset) {
        const std::uint8_t next = ByteAt(text, index + offset);
        if (!IsContinuation(next)) {
            return false;
        }
        code_point = (code_point << 6) | static_cast<std::uint32_t>(next & 0x3Fu);
    }

    if (code_point < minimum || code_point > 0x10FFFFu) {
        return false;
    }
    if (code_point >= 0xD800u && code_point <= 0xDFFFu) {
        return false;
    }

    index += length;
    return true;
}

void AppendUtf8(std::string& output, std::uint32_t code_point) {
    if (code_point <= 0x7Fu) {
        output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7FFu) {
        output.push_back(static_cast<char>(0xC0u | (code_point >> 6)));
        output.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    } else if (code_point <= 0xFFFFu) {
        output.push_back(static_cast<char>(0xE0u | (code_point >> 12)));
        output.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    } else {
        output.push_back(static_cast<char>(0xF0u | (code_point >> 18)));
        output.push_back(static_cast<char>(0x80u | ((code_point >> 12) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    }
}

nopeui::Result<std::u16string> InvalidUtf8(std::size_t offset) {
    return nopeui::Result<std::u16string>::Fail(
        nopeui::Error{nopeui::ErrorCode::InvalidUtf8, "invalid UTF-8 at byte " + std::to_string(offset)});
}

nopeui::Result<std::string> InvalidUtf16(std::size_t offset) {
    return nopeui::Result<std::string>::Fail(
        nopeui::Error{nopeui::ErrorCode::InvalidUtf16, "invalid UTF-16 at code unit " + std::to_string(offset)});
}

}  // namespace

namespace nopeui {

Result<std::u16string> Utf8ToUtf16(std::string_view text) {
    std::u16string output;
    output.reserve(text.size());

    std::size_t index = 0;
    while (index < text.size()) {
        std::uint32_t code_point = 0;
        if (!DecodeUtf8(text, index, code_point)) {
            return InvalidUtf8(index);
        }

        if (code_point <= 0xFFFFu) {
            output.push_back(static_cast<char16_t>(code_point));
        } else {
            const std::uint32_t offset = code_point - 0x10000u;
            output.push_back(static_cast<char16_t>(0xD800u + (offset >> 10)));
            output.push_back(static_cast<char16_t>(0xDC00u + (offset & 0x3FFu)));
        }
    }

    return Result<std::u16string>::Ok(std::move(output));
}

Result<std::string> Utf16ToUtf8(std::u16string_view text) {
    std::string output;
    output.reserve(text.size());

    std::size_t index = 0;
    while (index < text.size()) {
        const char16_t unit = text[index];
        std::uint32_t code_point = 0;

        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            if (index + 1 >= text.size()) {
                return InvalidUtf16(index);
            }
            const char16_t low = text[index + 1];
            if (low < 0xDC00u || low > 0xDFFFu) {
                return InvalidUtf16(index);
            }

            const std::uint32_t high_value = static_cast<std::uint32_t>(unit) - 0xD800u;
            const std::uint32_t low_value = static_cast<std::uint32_t>(low) - 0xDC00u;
            code_point = 0x10000u + (high_value << 10) + low_value;
            index += 2;
        } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            return InvalidUtf16(index);
        } else {
            code_point = static_cast<std::uint32_t>(unit);
            ++index;
        }

        AppendUtf8(output, code_point);
    }

    return Result<std::string>::Ok(std::move(output));
}

}  // namespace nopeui
