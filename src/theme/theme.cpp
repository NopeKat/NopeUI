#include "nopeui/theme/theme.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

namespace nopeui {

const Theme& LightTheme() noexcept {
    static const Theme theme{};
    return theme;
}

const Theme& DarkTheme() noexcept {
    static const Theme theme{[] {
        Theme dark;
        dark.background = 0x202020;
        dark.surface = 0x2B2B2B;
        dark.surface_border = 0x3A3A3A;
        dark.text = 0xFFFFFF;
        dark.text_secondary = 0xC8C8C8;
        dark.accent = 0x60CDFF;
        dark.button = 0x333333;
        dark.button_hover = 0x3D3D3D;
        dark.button_pressed = 0x2A2A2A;
        dark.button_border = 0x4A4A4A;
        return dark;
    }()};
    return theme;
}

Theme SystemTheme() noexcept {
    DWORD light = 1;
    DWORD size = sizeof(light);
    const LONG result = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &light,
        &size);
    if (result == ERROR_SUCCESS && light == 0) {
        return DarkTheme();
    }
    return LightTheme();
}

}  // namespace nopeui
