#ifndef NOPEUI_GRAPHICS_TEXT_H_
#define NOPEUI_GRAPHICS_TEXT_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "nopeui/base/geometry.h"
#include "nopeui/base/result.h"

struct ID2D1RenderTarget;

namespace nopeui::graphics {

enum class TextAlignment {
    Leading,
    Center,
    Trailing,
};

struct TextStyle {
    float size = 14.0f;
    bool bold = false;
    std::wstring font_family = L"Segoe UI";
    TextAlignment alignment = TextAlignment::Leading;
    // Multiplier for the default line height; 0 keeps the font's natural spacing.
    float line_spacing = 0.0f;
    bool wrap = false;
    bool ellipsis = false;
};

inline bool operator==(const TextStyle& left, const TextStyle& right) noexcept {
    return left.size == right.size &&
           left.bold == right.bold &&
           left.font_family == right.font_family &&
           left.alignment == right.alignment &&
           left.line_spacing == right.line_spacing &&
           left.wrap == right.wrap &&
           left.ellipsis == right.ellipsis;
}

inline bool operator!=(const TextStyle& left, const TextStyle& right) noexcept {
    return !(left == right);
}

// One laid-out run of text with its format cached. Measure() is valid after
// Create. max_width bounds wrapping, alignment and trimming; it does not clip
// drawing.
class TextRun {
public:
    static Result<std::unique_ptr<TextRun>> Create(
        std::wstring_view text,
        const TextStyle& style,
        float max_width = 8192.0f) noexcept;

    ~TextRun();

    Size Measure() const noexcept;

    // Horizontal caret position for a character index, in layout coordinates.
    float CaretX(std::size_t index) const noexcept;

    // Nearest character boundary to a layout-space x coordinate.
    std::size_t IndexFromX(float x) const noexcept;

    void Draw(ID2D1RenderTarget* target, Point origin, std::uint32_t color) const noexcept;

private:
    TextRun() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nopeui::graphics

#endif  // NOPEUI_GRAPHICS_TEXT_H_
