#include "nopeui/theme/theme.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <string_view>
#include <unordered_map>

namespace nopeui {

namespace {

using ResourceLookup = std::uint32_t (*)(const Theme&);
using ResourceTable = std::unordered_map<std::wstring, ResourceLookup>;

const ResourceTable& Palette() {
    static const ResourceTable table = {
        {L"Background", +[](const Theme& theme) { return theme.background; }},
        {L"Surface", +[](const Theme& theme) { return theme.surface; }},
        {L"SurfaceBorder", +[](const Theme& theme) { return theme.surface_border; }},
        {L"Text", +[](const Theme& theme) { return theme.text; }},
        {L"TextSecondary", +[](const Theme& theme) { return theme.text_secondary; }},
        {L"Accent", +[](const Theme& theme) { return theme.accent; }},
        {L"Button", +[](const Theme& theme) { return theme.button; }},
        {L"ButtonHover", +[](const Theme& theme) { return theme.button_hover; }},
        {L"ButtonPressed", +[](const Theme& theme) { return theme.button_pressed; }},
        {L"ButtonBorder", +[](const Theme& theme) { return theme.button_border; }},
        {L"TextDisabled", +[](const Theme& theme) { return theme.text_disabled; }},
        {L"ButtonDisabled", +[](const Theme& theme) { return theme.button_disabled; }},
        {L"ButtonDisabledBorder",
         +[](const Theme& theme) { return theme.button_disabled_border; }},
        {L"ScrollbarTrack", +[](const Theme& theme) { return theme.scrollbar_track; }},
        {L"ScrollbarThumb", +[](const Theme& theme) { return theme.scrollbar_thumb; }},
    };
    return table;
}

}  // namespace

bool Theme::FindThemeResource(std::wstring_view key, PropertyValue* value) const noexcept {
    const ResourceTable& table = Palette();
    const auto found = table.find(std::wstring(key));
    if (found == table.end()) {
        return false;
    }
    *value = PropertyValue(found->second(*this));
    return true;
}

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
        dark.text_disabled = 0x8A8A8A;
        dark.button_disabled = 0x2B2B2B;
        dark.button_disabled_border = 0x3A3A3A;
        dark.scrollbar_track = 0x303030;
        dark.scrollbar_thumb = 0x5A5A5A;
        return dark;
    }()};
    return theme;
}

const Theme& HighContrastTheme() noexcept {
    static const Theme theme{[] {
        Theme contrast;
        contrast.background = 0x000000;
        contrast.surface = 0x000000;
        contrast.surface_border = 0xFFFFFF;
        contrast.text = 0xFFFFFF;
        contrast.text_secondary = 0xFFFFFF;
        contrast.accent = 0xFFFF00;
        contrast.button = 0x000000;
        contrast.button_hover = 0x1A1A1A;
        contrast.button_pressed = 0x333333;
        contrast.button_border = 0xFFFFFF;
        contrast.text_disabled = 0x00FF00;
        contrast.button_disabled = 0x000000;
        contrast.button_disabled_border = 0x00FF00;
        contrast.scrollbar_track = 0x000000;
        contrast.scrollbar_thumb = 0xFFFFFF;
        return contrast;
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
