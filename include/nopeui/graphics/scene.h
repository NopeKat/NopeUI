#ifndef NOPEUI_GRAPHICS_SCENE_H_
#define NOPEUI_GRAPHICS_SCENE_H_

#include <cstdint>
#include <memory>

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

// Holds the brushes and text layout of the placeholder scene so a frame does
// not rebuild them. Brushes are device resources; one painter can serve all
// targets of the same device.
class ScenePainter {
public:
    static Result<std::unique_ptr<ScenePainter>> Create(
        ID2D1RenderTarget* target,
        const ScenePalette& palette) noexcept;

    ~ScenePainter();

    // Fills the given area with the background, draws the text if the area
    // intersects it, and draws the accent strip if the area reaches the top.
    Result<void> Paint(
        float width,
        float height,
        float left,
        float top,
        float right,
        float bottom) noexcept;

private:
    ScenePainter() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Creates a temporary painter and paints the whole surface. Used by tools and
// by the software fallback.
Result<void> PaintPlaceholderScene(
    ID2D1RenderTarget* target,
    const ScenePalette& palette,
    float width,
    float height) noexcept;

}  // namespace nopeui::graphics

#endif  // NOPEUI_GRAPHICS_SCENE_H_
