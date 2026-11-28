#include "../test_check.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "nopeui/graphics/hardware.h"
#include "nopeui/graphics/image.h"
#include "nopeui/graphics/offscreen.h"
#include "nopeui/graphics/scene.h"
#include "nopeui/graphics/text.h"

namespace {

std::string TempPath(const char* name) {
    char directory[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, directory) == 0) {
        return std::string(name);
    }
    return std::string(directory) + name;
}

}  // namespace

int main() {
    // Invalid sizes and dpi are rejected before any device work.
    {
        NOPEUI_CHECK(!nopeui::graphics::OffscreenSurface::Create(0, 32).ok());
        NOPEUI_CHECK(!nopeui::graphics::OffscreenSurface::Create(32, -1).ok());
        NOPEUI_CHECK(!nopeui::graphics::OffscreenSurface::Create(32, 32, 0.0f).ok());
        NOPEUI_CHECK(!nopeui::graphics::HardwareSurface::Create(0, 32).ok());
        NOPEUI_CHECK(!nopeui::graphics::HardwareSurface::Create(32, 32, -1.0f).ok());
    }

    // Text runs cover the styles the label and text box use.
    {
        using nopeui::graphics::TextAlignment;
        using nopeui::graphics::TextRun;
        using nopeui::graphics::TextStyle;

        NOPEUI_CHECK(!TextRun::Create(L"", TextStyle{}).ok());
        TextStyle too_small;
        too_small.size = 0.0f;
        NOPEUI_CHECK(!TextRun::Create(L"text", too_small).ok());

        TextStyle plain;
        auto run = TextRun::Create(L"NopeUI", plain);
        NOPEUI_CHECK(run.ok());
        NOPEUI_CHECK(run.value()->Measure().width > 0.0f);
        NOPEUI_CHECK(run.value()->Measure().height > 0.0f);
        NOPEUI_CHECK(run.value()->CaretX(0) == 0.0f);
        NOPEUI_CHECK(run.value()->CaretX(100) >= 0.0f);
        NOPEUI_CHECK(run.value()->IndexFromX(-10.0f) == 0);
        NOPEUI_CHECK(run.value()->IndexFromX(10000.0f) <= 6);
        run.value()->Draw(nullptr, nopeui::Point{0.0f, 0.0f}, 0x000000);

        TextStyle styled;
        styled.bold = true;
        styled.font_family = L"Missing Font Family";
        styled.alignment = TextAlignment::Center;
        styled.line_spacing = 1.5f;
        styled.wrap = true;
        styled.ellipsis = true;
        auto styled_run = TextRun::Create(L"A longer line of text", styled, 80.0f);
        NOPEUI_CHECK(styled_run.ok());
        NOPEUI_CHECK(styled_run.value()->Measure().width <= 81.0f);

        TextStyle trailing;
        trailing.alignment = TextAlignment::Trailing;
        auto trailing_run = TextRun::Create(L"Trailing", trailing);
        NOPEUI_CHECK(trailing_run.ok());

        TextStyle narrow;
        narrow.wrap = true;
        auto narrow_run = TextRun::Create(L"one two three four", narrow, 0.5f);
        NOPEUI_CHECK(narrow_run.ok());
        NOPEUI_CHECK(nopeui::graphics::TextStyle{} == nopeui::graphics::TextStyle{});
        narrow.size = 20.0f;
        NOPEUI_CHECK(narrow != nopeui::graphics::TextStyle{});
    }

    // PNG round trip keeps size, order and alpha.
    {
        const std::string path = TempPath("nopeui-graphics-round-trip.png");
        std::vector<std::uint32_t> pixels{
            0xFF0000FFu, 0x8000FF00u, 0x400000FFu, 0x00FFFFFFu};
        NOPEUI_CHECK(nopeui::graphics::SavePng(path, 2, 2, pixels).ok());

        auto loaded = nopeui::graphics::LoadPng(path);
        NOPEUI_CHECK(loaded.ok());
        NOPEUI_CHECK(loaded.value().width == 2);
        NOPEUI_CHECK(loaded.value().height == 2);
        NOPEUI_CHECK(!loaded.value().Empty());
        NOPEUI_CHECK(loaded.value().pixels == pixels);

        // Saving again overwrites the existing file instead of appending.
        std::vector<std::uint32_t> single{0xFF00FFFFu};
        NOPEUI_CHECK(nopeui::graphics::SavePng(path, 1, 1, single).ok());
        auto smaller = nopeui::graphics::LoadPng(path);
        NOPEUI_CHECK(smaller.ok());
        NOPEUI_CHECK(smaller.value().width == 1);
        NOPEUI_CHECK(smaller.value().height == 1);
        DeleteFileA(path.c_str());

        // Bad arguments and missing files fail with a useful code.
        NOPEUI_CHECK(!nopeui::graphics::SavePng(path, 0, 4, pixels).ok());
        NOPEUI_CHECK(!nopeui::graphics::SavePng(path, 1, 1, {}).ok());
        NOPEUI_CHECK(!nopeui::graphics::SavePng(std::string_view{}, 1, 1, pixels).ok());
        NOPEUI_CHECK(!nopeui::graphics::SavePng(std::string("\xFF\xFE", 2), 1, 1, pixels).ok());
        auto missing = nopeui::graphics::LoadPng(path);
        NOPEUI_CHECK(!missing.ok());
        NOPEUI_CHECK(missing.error().code == nopeui::ErrorCode::NotFound);
        NOPEUI_CHECK(!nopeui::graphics::LoadPng(std::string_view{}).ok());

        nopeui::graphics::Image empty;
        NOPEUI_CHECK(empty.Empty());
        empty.width = 2;
        empty.height = 1;
        empty.pixels.push_back(0);
        NOPEUI_CHECK(empty.Empty());

        const std::string blocked = std::string("Z:\\nopeui-tests\\no-such-dir\\x.png");
        std::vector<std::uint32_t> one{0};
        NOPEUI_CHECK(!nopeui::graphics::SavePng(blocked, 1, 1, one).ok());
    }

    // Offscreen target drives the placeholder scene and a saved frame.
    {
        auto surface_result = nopeui::graphics::OffscreenSurface::Create(96, 64);
        NOPEUI_CHECK(surface_result.ok());
        auto& surface = surface_result.value();
        NOPEUI_CHECK(surface->Width() == 96);
        NOPEUI_CHECK(surface->Height() == 64);
        NOPEUI_CHECK(surface->Target() != nullptr);
        NOPEUI_CHECK(surface->WicBitmap() != nullptr);

        NOPEUI_CHECK(surface->BeginDraw().ok());
        NOPEUI_CHECK(nopeui::graphics::PaintPlaceholderScene(
                         surface->Target(),
                         nopeui::graphics::LightPalette(),
                         96.0f,
                         64.0f)
                         .ok());
        NOPEUI_CHECK(surface->EndDraw().ok());

        const std::string path = TempPath("nopeui-graphics-scene.png");
        NOPEUI_CHECK(surface->SavePng(path).ok());
        auto loaded = nopeui::graphics::LoadPng(path);
        NOPEUI_CHECK(loaded.ok());
        NOPEUI_CHECK(loaded.value().width == 96);
        NOPEUI_CHECK(loaded.value().height == 64);
        NOPEUI_CHECK(!loaded.value().Empty());

        // The lower area is background, the top strip is the accent.
        const std::uint32_t background = loaded.value().pixels[40 * 96];
        const std::uint32_t accent = loaded.value().pixels[10];
        NOPEUI_CHECK(background != accent);
        DeleteFileA(path.c_str());

        // Painter is reusable across areas and targets of the same device.
        auto painter = nopeui::graphics::ScenePainter::Create(
            surface->Target(), nopeui::graphics::DarkPalette());
        NOPEUI_CHECK(painter.ok());
        NOPEUI_CHECK(!nopeui::graphics::ScenePainter::Create(nullptr, {}).ok());

        NOPEUI_CHECK(surface->BeginDraw().ok());
        NOPEUI_CHECK(painter.value()
                         ->Paint(96.0f, 64.0f, 0.0f, 0.0f, 96.0f, 64.0f)
                         .ok());
        // An area that misses the text and the top strip paints just background.
        NOPEUI_CHECK(painter.value()
                         ->Paint(96.0f, 64.0f, 80.0f, 40.0f, 95.0f, 60.0f)
                         .ok());
        // A top strip area still draws the accent bar.
        NOPEUI_CHECK(painter.value()
                         ->Paint(96.0f, 64.0f, 0.0f, 0.0f, 96.0f, 20.0f)
                         .ok());
        // Empty or inverted rectangles are no-ops.
        NOPEUI_CHECK(painter.value()
                         ->Paint(96.0f, 64.0f, 10.0f, 10.0f, 10.0f, 20.0f)
                         .ok());
        NOPEUI_CHECK(painter.value()
                         ->Paint(0.0f, 64.0f, 0.0f, 0.0f, 10.0f, 20.0f)
                         .ok());
        NOPEUI_CHECK(surface->EndDraw().ok());

        // Text draws through the target as well.
        auto run = nopeui::graphics::TextRun::Create(
            L"NopeUI", nopeui::graphics::TextStyle{});
        NOPEUI_CHECK(run.ok());
        NOPEUI_CHECK(surface->BeginDraw().ok());
        run.value()->Draw(surface->Target(), nopeui::Point{4.0f, 4.0f}, 0x1B1B1B);
        NOPEUI_CHECK(surface->EndDraw().ok());

        // A second surface at another dpi is independent.
        auto hidpi = nopeui::graphics::OffscreenSurface::Create(40, 30, 144.0f);
        NOPEUI_CHECK(hidpi.ok());
        NOPEUI_CHECK(hidpi.value()->Width() == 40);
        NOPEUI_CHECK(hidpi.value()->BeginDraw().ok());
        NOPEUI_CHECK(hidpi.value()->EndDraw().ok());
    }

    // Hardware target covers the same frame through D2D on D3D11.
    {
        auto surface_result = nopeui::graphics::HardwareSurface::Create(64, 48);
        NOPEUI_CHECK(surface_result.ok());
        auto& surface = surface_result.value();
        NOPEUI_CHECK(surface->Width() == 64);
        NOPEUI_CHECK(surface->Height() == 48);
        NOPEUI_CHECK(surface->Target() != nullptr);
        // Hardware is expected here; WARP is only the fallback.
        NOPEUI_CHECK(!surface->IsWarp());

        NOPEUI_CHECK(surface->BeginDraw().ok());
        NOPEUI_CHECK(nopeui::graphics::PaintPlaceholderScene(
                         surface->Target(),
                         nopeui::graphics::DarkPalette(),
                         64.0f,
                         48.0f)
                         .ok());
        NOPEUI_CHECK(surface->EndDraw().ok());

        const std::string path = TempPath("nopeui-graphics-hardware.png");
        NOPEUI_CHECK(surface->SavePng(path).ok());
        auto loaded = nopeui::graphics::LoadPng(path);
        NOPEUI_CHECK(loaded.ok());
        NOPEUI_CHECK(loaded.value().width == 64);
        NOPEUI_CHECK(loaded.value().height == 48);
        NOPEUI_CHECK(loaded.value().pixels.size() == 64u * 48u);
        DeleteFileA(path.c_str());
    }

    return 0;
}
