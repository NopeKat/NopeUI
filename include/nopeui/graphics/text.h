#ifndef NOPEUI_GRAPHICS_TEXT_H_
#define NOPEUI_GRAPHICS_TEXT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "nopeui/base/geometry.h"
#include "nopeui/base/result.h"

struct ID2D1RenderTarget;

namespace nopeui::graphics {

struct TextStyle {
    float size = 14.0f;
    bool bold = false;
    bool wrap = false;
    bool ellipsis = false;
};

// One laid-out run of text with its format cached. Measure() is valid after
// Create. max_width bounds wrapping and trimming; it does not clip drawing.
class TextRun {
public:
    static Result<std::unique_ptr<TextRun>> Create(
        std::wstring_view text,
        const TextStyle& style,
        float max_width = 8192.0f) noexcept;

    ~TextRun();

    Size Measure() const noexcept;
    void Draw(ID2D1RenderTarget* target, Point origin, std::uint32_t color) const noexcept;

private:
    TextRun() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nopeui::graphics

#endif  // NOPEUI_GRAPHICS_TEXT_H_
