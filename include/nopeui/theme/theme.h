#ifndef NOPEUI_THEME_THEME_H_
#define NOPEUI_THEME_THEME_H_

#include <cstdint>
#include <string_view>

#include "nopeui/base/property_value.h"

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
    std::uint32_t text_disabled = 0x9A9A9A;
    std::uint32_t button_disabled = 0xF0F0F0;
    std::uint32_t button_disabled_border = 0xDFDFDF;
    std::uint32_t scrollbar_track = 0xE8E8E8;
    std::uint32_t scrollbar_thumb = 0xC0C0C0;

    // Theme-owned resources. Keys are the palette names ("Accent", "Text",
    // "SurfaceBorder", ...) and the value is always a color, so markup can say
    // {ThemeResource Accent} and keep following theme switches.
    bool FindThemeResource(std::wstring_view key, PropertyValue* value) const noexcept;
};

const Theme& LightTheme() noexcept;
const Theme& DarkTheme() noexcept;
// System high contrast palette: black canvas, white text and a yellow accent,
// matching the classic Windows contrast scheme more closely than dark does.
const Theme& HighContrastTheme() noexcept;

// Follows the Windows "app mode" preference; falls back to light.
Theme SystemTheme() noexcept;

}  // namespace nopeui

#endif  // NOPEUI_THEME_THEME_H_
