#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "nopeui/graphics/hardware.h"
#include "nopeui/graphics/offscreen.h"
#include "nopeui/graphics/scene.h"

namespace {

void PrintUsage() {
    std::fprintf(
        stderr,
        "usage: nopeui_golden --out <path> [--renderer software|hardware] "
        "[--theme light|dark] [--width 1280] [--height 800] [--dpi 96]\n");
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

template <typename Surface>
int RenderAndSave(
    Surface& surface,
    const nopeui::graphics::ScenePalette& palette,
    const char* renderer_name,
    const std::string& theme,
    int width,
    int height,
    float dpi,
    const std::string& out) {
    auto begin_result = surface.BeginDraw();
    if (!begin_result.ok()) {
        std::fprintf(stderr, "%s\n", begin_result.error().message.c_str());
        return 4;
    }
    auto painted = nopeui::graphics::PaintPlaceholderScene(
        surface.Target(),
        palette,
        static_cast<float>(width),
        static_cast<float>(height));
    auto end_result = surface.EndDraw();
    if (!painted.ok()) {
        std::fprintf(stderr, "%s\n", painted.error().message.c_str());
        return 4;
    }
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
        "golden: %dx%d theme=%s renderer=%s dpi=%.0f -> %s\n",
        width,
        height,
        theme.c_str(),
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

    const nopeui::graphics::ScenePalette palette = theme == "dark"
        ? nopeui::graphics::DarkPalette()
        : nopeui::graphics::LightPalette();

    if (renderer == "software") {
        auto surface_result = nopeui::graphics::OffscreenSurface::Create(width, height, dpi);
        if (!surface_result.ok()) {
            std::fprintf(stderr, "%s\n", surface_result.error().message.c_str());
            return 3;
        }
        auto& surface = surface_result.value();
        return RenderAndSave(*surface, palette, "software", theme, width, height, dpi, out);
    }

    auto surface_result = nopeui::graphics::HardwareSurface::Create(width, height, dpi, true);
    if (!surface_result.ok()) {
        std::fprintf(stderr, "%s\n", surface_result.error().message.c_str());
        return 3;
    }
    auto& surface = surface_result.value();
    return RenderAndSave(*surface, palette, "hardware", theme, width, height, dpi, out);
}
