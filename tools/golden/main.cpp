#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/graphics/hardware.h"
#include "nopeui/graphics/offscreen.h"
#include "nopeui/graphics/scene.h"
#include "nopeui/layout/element.h"
#include "nopeui/theme/theme.h"

namespace {

void PrintUsage() {
    std::fprintf(
        stderr,
        "usage: nopeui_golden --out <path> [--renderer software|hardware] "
        "[--scene placeholder|controls] [--theme light|dark] "
        "[--width 1280] [--height 800] [--dpi 96]\n");
}

bool ParseInt(const char* text, int* value) {
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == nullptr || *end != '\0' || parsed <= 0 || parsed > 16384) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool ParseFloat(const char* text, float* value) {
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == nullptr || *end != '\0' || parsed <= 0.0 || parsed > 768.0) {
        return false;
    }
    *value = static_cast<float>(parsed);
    return true;
}

nopeui::Ref<nopeui::Element> BuildControlsScene(
    const nopeui::Theme& theme,
    int width,
    int height) {
    auto page = nopeui::Border::Create();
    page->SetBorderVisible(false);
    page->SetPadding(nopeui::Thickness{24.0f, 24.0f, 24.0f, 24.0f});

    auto card = nopeui::Border::Create();
    card->UseSurfaceBackground(true);
    card->SetPadding(nopeui::Thickness{24.0f, 20.0f, 24.0f, 20.0f});

    auto panel = nopeui::StackPanel::Create(12.0f);
    panel->Add(nopeui::Label::Create(L"NopeUI controls"));
    panel->Add(nopeui::Label::Create(L"A fixed control tree for golden images"));

    auto row = nopeui::StackPanel::Create(10.0f);
    row->SetOrientation(nopeui::Orientation::Horizontal);
    row->Add(nopeui::Button::Create(L"Light"));
    row->Add(nopeui::Button::Create(L"Dark"));
    panel->Add(row);

    panel->Add(nopeui::Label::Create(L"Spacing, borders and text share one theme"));

    nopeui::graphics::TextStyle body;
    body.wrap = true;
    panel->Add(nopeui::Label::Create(
        L"Wrapped text is measured against the card width, so this paragraph "
        L"occupies more than one line and still stays inside the layout. "
        L"Resize the window and the line breaks follow the available width "
        L"without rebuilding the whole tree.",
        body));

    nopeui::graphics::TextStyle single;
    single.ellipsis = true;
    panel->Add(nopeui::Label::Create(
        L"Single line text that is deliberately long enough to run past the "
        L"card width and get trimmed with an ellipsis at the edge of the "
        L"available space instead of being clipped in the middle of a word. "
        L"The trimmed tail ends with a visible ellipsis when space runs out.",
        single));

    card->SetChild(panel);
    page->SetChild(card);
    page->SetTheme(&theme);

    const nopeui::Size available{
        static_cast<float>(width),
        static_cast<float>(height)};
    page->Measure(available);
    page->Arrange(nopeui::Rect{0.0f, 0.0f, available.width, available.height});
    return page;
}

template <typename Surface>
int RenderAndSave(
    Surface& surface,
    const std::string& scene,
    const nopeui::graphics::ScenePalette& palette,
    const nopeui::Theme& theme,
    const char* renderer_name,
    const std::string& theme_name,
    int width,
    int height,
    float dpi,
    const std::string& out) {
    nopeui::Ref<nopeui::Element> tree;
    if (scene == "controls") {
        tree = BuildControlsScene(theme, width, height);
    }

    auto begin_result = surface.BeginDraw();
    if (!begin_result.ok()) {
        std::fprintf(stderr, "%s\n", begin_result.error().message.c_str());
        return 4;
    }
    if (tree) {
        tree->Paint(surface.Target());
    } else {
        auto painted = nopeui::graphics::PaintPlaceholderScene(
            surface.Target(),
            palette,
            static_cast<float>(width),
            static_cast<float>(height));
        if (!painted.ok()) {
            std::fprintf(stderr, "%s\n", painted.error().message.c_str());
            return 4;
        }
    }
    auto end_result = surface.EndDraw();
    if (!end_result.ok()) {
        std::fprintf(stderr, "%s\n", end_result.error().message.c_str());
        return 4;
    }

    auto saved = surface.SavePng(out);
    if (!saved.ok()) {
        std::fprintf(stderr, "%s\n", saved.error().message.c_str());
        return 5;
    }

    std::printf(
        "golden: %dx%d scene=%s theme=%s renderer=%s dpi=%.0f -> %s\n",
        width,
        height,
        scene.c_str(),
        theme_name.c_str(),
        renderer_name,
        static_cast<double>(dpi),
        out.c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string out;
    std::string theme = "light";
    std::string renderer = "software";
    std::string scene = "placeholder";
    int width = 1280;
    int height = 800;
    float dpi = 96.0f;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--out" && index + 1 < argc) {
            out = argv[++index];
        } else if (argument == "--theme" && index + 1 < argc) {
            theme = argv[++index];
        } else if (argument == "--renderer" && index + 1 < argc) {
            renderer = argv[++index];
        } else if (argument == "--scene" && index + 1 < argc) {
            scene = argv[++index];
        } else if (argument == "--width" && index + 1 < argc) {
            if (!ParseInt(argv[++index], &width)) {
                PrintUsage();
                return 2;
            }
        } else if (argument == "--height" && index + 1 < argc) {
            if (!ParseInt(argv[++index], &height)) {
                PrintUsage();
                return 2;
            }
        } else if (argument == "--dpi" && index + 1 < argc) {
            if (!ParseFloat(argv[++index], &dpi)) {
                PrintUsage();
                return 2;
            }
        } else {
            PrintUsage();
            return 2;
        }
    }

    if (out.empty()) {
        PrintUsage();
        return 2;
    }
    if (theme != "light" && theme != "dark") {
        std::fprintf(stderr, "unknown theme '%s'\n", theme.c_str());
        return 2;
    }
    if (renderer != "software" && renderer != "hardware") {
        std::fprintf(stderr, "unknown renderer '%s'\n", renderer.c_str());
        return 2;
    }
    if (scene != "placeholder" && scene != "controls") {
        std::fprintf(stderr, "unknown scene '%s'\n", scene.c_str());
        return 2;
    }

    const nopeui::graphics::ScenePalette palette = theme == "dark"
        ? nopeui::graphics::DarkPalette()
        : nopeui::graphics::LightPalette();
    const nopeui::Theme theme_value = theme == "dark"
        ? nopeui::DarkTheme()
        : nopeui::LightTheme();

    if (renderer == "software") {
        auto surface_result = nopeui::graphics::OffscreenSurface::Create(width, height, dpi);
        if (!surface_result.ok()) {
            std::fprintf(stderr, "%s\n", surface_result.error().message.c_str());
            return 3;
        }
        auto& surface = surface_result.value();
        return RenderAndSave(
            *surface, scene, palette, theme_value, "software", theme, width, height, dpi, out);
    }

    auto surface_result = nopeui::graphics::HardwareSurface::Create(width, height, dpi, true);
    if (!surface_result.ok()) {
        std::fprintf(stderr, "%s\n", surface_result.error().message.c_str());
        return 3;
    }
    auto& surface = surface_result.value();
    return RenderAndSave(
        *surface, scene, palette, theme_value, "hardware", theme, width, height, dpi, out);
}
