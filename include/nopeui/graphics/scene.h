#ifndef NOPEUI_GRAPHICS_SCENE_H_
#define NOPEUI_GRAPHICS_SCENE_H_

#include <cstdint>

#include "nopeui/base/result.h"

struct ID2D1RenderTarget;

namespace nopeui::graphics {

struct ScenePalette {
    std::uint32_t background = 0xF3F3F3;
    std::uint32_t accent = 0x0067C0;
    std::uint32_t text = 0x1B1B1B;
};

ScenePalette LightPalette() noexcept;
ScenePalette DarkPalette() noexcept;

// Paints the placeholder window scene shared by the window layer and golden
// images: a flat background, a 4 DIP accent strip and one line of text.
Result<void> PaintPlaceholderScene(
    ID2D1RenderTarget* target,
    const ScenePalette& palette,
    float width,
    float height) noexcept;

}  // namespace nopeui::graphics

#endif  // NOPEUI_GRAPHICS_SCENE_H_
