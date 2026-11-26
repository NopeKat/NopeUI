#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/check_box.h"
#include "nopeui/controls/context_menu.h"
#include "nopeui/controls/flyout.h"
#include "nopeui/controls/image.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/progress_bar.h"
#include "nopeui/controls/radio_button.h"
#include "nopeui/controls/scroll_viewer.h"
#include "nopeui/controls/slider.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/controls/text_box.h"
#include "nopeui/controls/toggle_button.h"
#include "nopeui/controls/toggle_switch.h"
#include "nopeui/controls/tooltip.h"
#include "nopeui/graphics/hardware.h"
#include "nopeui/graphics/offscreen.h"
#include "nopeui/graphics/scene.h"
#include "nopeui/layout/element.h"
#include "nopeui/layout/style.h"
#include "nopeui/markup/xaml.h"
#include "nopeui/theme/control_template.h"
#include "nopeui/theme/theme.h"

namespace {

void PrintUsage() {
    std::fprintf(
        stderr,
        "usage: nopeui_golden --out <path> [--renderer software|hardware] "
        "[--scene placeholder|controls|media|xaml] [--theme light|dark] "
        "[--xaml <path>] [--width 1280] [--height 800] [--dpi 96]\n");
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
    card->SetCornerRadius(8.0f);
    card->SetPadding(nopeui::Thickness{24.0f, 20.0f, 24.0f, 20.0f});

    auto panel = nopeui::StackPanel::Create(12.0f);
    panel->Add(nopeui::Label::Create(L"NopeUI controls"));
    panel->Add(nopeui::Label::Create(L"A fixed control tree for golden images"));

    auto row = nopeui::StackPanel::Create(10.0f);
    row->SetOrientation(nopeui::Orientation::Horizontal);
    row->Add(nopeui::Button::Create(L"Light"));
    row->Add(nopeui::Button::Create(L"Dark"));
    panel->Add(row);

    auto states = nopeui::StackPanel::Create(10.0f);
    states->SetOrientation(nopeui::Orientation::Horizontal);
    auto enabled = nopeui::Button::Create(L"Enabled");
    auto disabled = nopeui::Button::Create(L"Disabled");
    disabled->SetEnabled(false);
    states->Add(enabled);
    states->Add(disabled);
    panel->Add(states);

    auto options = nopeui::StackPanel::Create(16.0f);
    options->SetOrientation(nopeui::Orientation::Horizontal);
    auto checked = nopeui::CheckBox::Create(L"Checked");
    checked->SetChecked(true);
    auto unchecked = nopeui::CheckBox::Create(L"Unchecked");
    options->Add(checked);
    options->Add(unchecked);
    auto off = nopeui::ToggleButton::Create(L"Toggle off");
    auto on = nopeui::ToggleButton::Create(L"Toggle on");
    on->SetChecked(true);
    options->Add(off);
    options->Add(on);
    panel->Add(options);

    auto radios = nopeui::StackPanel::Create(16.0f);
    radios->SetOrientation(nopeui::Orientation::Horizontal);
    auto radio_a = nopeui::RadioButton::Create(L"Radio A");
    auto radio_b = nopeui::RadioButton::Create(L"Radio B");
    radio_a->SetChecked(true);
    radios->Add(radio_a);
    radios->Add(radio_b);
    panel->Add(radios);

    auto switch_off = nopeui::ToggleSwitch::Create(L"Switch off");
    panel->Add(switch_off);
    auto switch_on = nopeui::ToggleSwitch::Create(L"Switch on");
    switch_on->SetChecked(true);
    panel->Add(switch_on);

    auto progress = nopeui::ProgressBar::Create(0.45f);
    progress->SetAccessibilityName(L"Progress");
    panel->Add(progress);

    auto slider = nopeui::Slider::Create(40.0, 0.0, 100.0);
    slider->SetAccessibilityName(L"Volume");
    panel->Add(slider);

    panel->Add(nopeui::TextBox::Create(L"Editable single line"));

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

    nopeui::graphics::TextStyle mono;
    mono.font_family = L"Consolas";
    panel->Add(nopeui::Label::Create(L"Consolas: const int value = 42;", mono));

    nopeui::graphics::TextStyle centered;
    centered.alignment = nopeui::graphics::TextAlignment::Center;
    panel->Add(nopeui::Label::Create(L"Centered text", centered));

    nopeui::graphics::TextStyle spaced;
    spaced.wrap = true;
    spaced.line_spacing = 1.4f;
    panel->Add(nopeui::Label::Create(
        L"A wrapped paragraph with wider line spacing than the default,\n"
        L"used to pin the line height multiplier in the golden image.",
        spaced));

    auto strip = nopeui::StackPanel::Create(8.0f);
    strip->SetOrientation(nopeui::Orientation::Horizontal);
    for (int index = 0; index < 24; ++index) {
        wchar_t item[32]{};
        std::swprintf(item, std::size(item), L"Item %02d", index);
        strip->Add(nopeui::Label::Create(item));
    }
    nopeui::graphics::TextStyle spacer;
    spacer.size = 8.0f;
    auto strip_content = nopeui::StackPanel::Create(0.0f);
    strip_content->Add(strip);
    strip_content->Add(nopeui::Label::Create(L" ", spacer));
    auto strip_scroll = nopeui::ScrollViewer::Create();
    strip_scroll->SetChild(strip_content);
    panel->Add(strip_scroll);

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

nopeui::graphics::Image MakeSampleImage() {
    nopeui::graphics::Image image;
    image.width = 320;
    image.height = 180;
    image.pixels.resize(
        static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height));
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const bool checker = ((x / 20) + (y / 20)) % 2 == 0;
            const std::uint32_t base = checker ? 0x0067C0u : 0x00A7C4u;
            const std::uint32_t mix = static_cast<std::uint32_t>(
                (static_cast<float>(x + y) / static_cast<float>(image.width + image.height)) *
                80.0f);
            const std::uint32_t red = ((base >> 16) & 0xffu) + mix;
            const std::uint32_t green = ((base >> 8) & 0xffu);
            const std::uint32_t blue = (base & 0xffu) + (80u - mix);
            std::uint32_t pixel = 0xff000000u |
                                  ((red > 255u ? 255u : red) << 16) |
                                  (green << 8) |
                                  (blue > 255u ? 255u : blue);
            if (x > 246 && y < 74) {
                pixel = 0xffffffffu;
            }
            image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                         static_cast<std::size_t>(x)] = pixel;
        }
    }
    return image;
}

nopeui::Ref<nopeui::Element> BuildMediaScene(
    const nopeui::Theme& theme,
    int width,
    int height) {
    auto page = nopeui::Border::Create();
    page->SetBorderVisible(false);
    page->SetPadding(nopeui::Thickness{24.0f, 24.0f, 24.0f, 24.0f});

    auto card = nopeui::Border::Create();
    card->UseSurfaceBackground(true);
    card->SetCornerRadius(8.0f);
    card->SetPadding(nopeui::Thickness{24.0f, 20.0f, 24.0f, 20.0f});

    auto panel = nopeui::StackPanel::Create(14.0f);
    panel->Add(nopeui::Label::Create(L"NopeUI media"));
    panel->Add(nopeui::Label::Create(L"Bitmap stretch modes and popup surfaces"));

    auto modes = nopeui::StackPanel::Create(12.0f);
    modes->SetOrientation(nopeui::Orientation::Horizontal);
    const struct {
        const wchar_t* label;
        nopeui::ImageStretch stretch;
    } mode_list[] = {
        {L"None", nopeui::ImageStretch::None},
        {L"Fill", nopeui::ImageStretch::Fill},
        {L"Uniform", nopeui::ImageStretch::Uniform},
        {L"UniformToFill", nopeui::ImageStretch::UniformToFill},
    };
    for (const auto& mode : mode_list) {
        auto tile = nopeui::Border::Create();
        tile->UseSurfaceBackground(true);
        tile->SetBorderVisible(true);
        tile->SetCornerRadius(6.0f);
        tile->SetPadding(nopeui::Thickness{10.0f, 8.0f, 10.0f, 8.0f});
        auto column = nopeui::StackPanel::Create(6.0f);
        column->Add(nopeui::Label::Create(mode.label));
        auto photo = nopeui::Image::Create();
        photo->SetImage(MakeSampleImage());
        photo->SetDisplaySize(nopeui::Size{200.0f, 150.0f});
        photo->SetStretch(mode.stretch);
        column->Add(photo);
        tile->SetChild(column);
        modes->Add(tile);
    }
    panel->Add(modes);

    auto popup_row = nopeui::StackPanel::Create(12.0f);
    popup_row->SetOrientation(nopeui::Orientation::Horizontal);
    popup_row->Add(nopeui::ToolTip::Create(L"ToolTip surface"));

    auto flyout_content = nopeui::StackPanel::Create(8.0f);
    flyout_content->Add(nopeui::Label::Create(L"Flyout"));
    flyout_content->Add(nopeui::Button::Create(L"Action"));
    auto flyout = nopeui::Flyout::Create();
    flyout->SetContent(flyout_content);
    popup_row->Add(flyout);

    auto menu = nopeui::ContextMenu::Create();
    menu->AddItem(L"Copy", [] {});
    menu->AddItem(L"Rename", [] {});
    menu->AddItem(L"Remove", [] {});
    popup_row->Add(menu);
    panel->Add(popup_row);

    auto primary = nopeui::Button::Create(L"Templated button");
    primary->SetKey(L"primary");
    panel->Add(primary);

    auto primary_style = nopeui::Style::Create();
    primary_style->SetTargetType(L"Button");
    primary_style->SetTargetKey(L"primary");
    auto primary_template = nopeui::ControlTemplate::Create();
    nopeui::TemplatePart primary_background;
    primary_background.background = 0x0067C0;
    primary_background.border = 0x005A9E;
    primary_background.corner_radius = 6.0f;
    primary_template->SetPart(L"Background", primary_background);
    nopeui::TemplatePart primary_content;
    primary_content.foreground = 0xFFFFFF;
    primary_template->SetPart(L"Content", primary_content);
    primary_style->SetTemplate(primary_template);
    auto sheet = nopeui::StyleSheet::Create();
    sheet->Add(primary_style);

    card->SetChild(panel);
    page->SetChild(card);
    page->SetTheme(&theme);
    sheet->ApplyTo(*page);

    const nopeui::Size available{
        static_cast<float>(width),
        static_cast<float>(height)};
    page->Measure(available);
    page->Arrange(nopeui::Rect{0.0f, 0.0f, available.width, available.height});
    return page;
}

nopeui::Ref<nopeui::Element> BuildXamlScene(
    const nopeui::Theme& theme,
    int width,
    int height,
    const std::string& xaml_path) {
    if (xaml_path.empty()) {
        std::fprintf(stderr, "xaml scene requires --xaml <path>\n");
        return {};
    }
    nopeui::xaml::LoadContext context;
    context.resolve_event = [](std::wstring_view) {
        return nopeui::xaml::EventHandler(
            [](const nopeui::xaml::EventArgs&) {});
    };
    auto loaded = nopeui::xaml::LoadFile(xaml_path, context);
    if (!loaded.ok()) {
        std::fprintf(stderr, "xaml load failed: %s\n", loaded.error().message.c_str());
        return {};
    }
    auto page = std::move(loaded).value();
    page.root->SetTheme(&theme);
    const nopeui::Size available{
        static_cast<float>(width),
        static_cast<float>(height)};
    page.root->Measure(available);
    page.root->Arrange(nopeui::Rect{0.0f, 0.0f, available.width, available.height});
    return page.root;
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
    const std::string& xaml_path,
    const std::string& out) {
    nopeui::Ref<nopeui::Element> tree;
    if (scene == "controls") {
        tree = BuildControlsScene(theme, width, height);
    } else if (scene == "media") {
        tree = BuildMediaScene(theme, width, height);
    } else if (scene == "xaml") {
        tree = BuildXamlScene(theme, width, height, xaml_path);
    }

    if (scene == "xaml" && !tree) {
        return 4;
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
    std::string xaml_path;
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
        } else if (argument == "--xaml" && index + 1 < argc) {
            xaml_path = argv[++index];
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
    if (scene != "placeholder" && scene != "controls" && scene != "media" &&
        scene != "xaml") {
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
            *surface, scene, palette, theme_value, "software", theme, width, height, dpi,
            xaml_path, out);
    }

    auto surface_result = nopeui::graphics::HardwareSurface::Create(width, height, dpi, true);
    if (!surface_result.ok()) {
        std::fprintf(stderr, "%s\n", surface_result.error().message.c_str());
        return 3;
    }
    auto& surface = surface_result.value();
    return RenderAndSave(
        *surface, scene, palette, theme_value, "hardware", theme, width, height, dpi,
        xaml_path, out);
}
