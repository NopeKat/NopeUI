#ifndef NOPEUI_THEME_THEME_H_
#define NOPEUI_THEME_THEME_H_

#include <cstdint>

namespace nopeui {

// Colors are 0xRRGGBB, matching the Direct2D ColorF(UINT32) constructor.
struct Theme {
    std::uint32_t background = 0xF3F3F3;
    std::uint32_t surface = 0xFFFFFF;
    std::uint32_t surface_border = 0xE1E1E1;
    std::uint32_t text = 0x1B1B1B;
    std::uint32_t text_secondary = 0x5F5F5F;
    std::uint32_t accent = 0x0067C0;
    std::uint32_t button = 0xEBEBEB;
    std::uint32_t button_hover = 0xE2E2E2;
    std::uint32_t button_pressed = 0xD4D4D4;
    std::uint32_t button_border = 0xCFCFCF;
};

const Theme& LightTheme() noexcept;
const Theme& DarkTheme() noexcept;

// Follows the Windows "app mode" preference; falls back to light.
Theme SystemTheme() noexcept;

}  // namespace nopeui

#endif  // NOPEUI_THEME_THEME_H_
