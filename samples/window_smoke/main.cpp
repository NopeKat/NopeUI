#include <atomic>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <string>
#include <thread>
#include <utility>

#include <Windows.h>

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
#include "nopeui/layout/style.h"
#include "nopeui/platform/window.h"
#include "nopeui/theme/control_template.h"
#include "nopeui/theme/theme.h"

namespace {

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

std::wstring FindTracePath(std::wstring_view command_line) {
    const std::wstring_view marker = L"--trace";
    const size_t marker_pos = command_line.find(marker);
    if (marker_pos == std::wstring_view::npos) {
        return {};
    }

    size_t pos = marker_pos + marker.size();
    while (pos < command_line.size() && command_line[pos] == L' ') {
        ++pos;
    }
    if (pos >= command_line.size()) {
        return {};
    }
    if (command_line[pos] == L'"') {
        const size_t end = command_line.find(L'"', pos + 1);
        if (end == std::wstring_view::npos) {
            return {};
        }
        return std::wstring(command_line.substr(pos + 1, end - pos - 1));
    }

    const size_t end = command_line.find(L' ', pos);
    return std::wstring(command_line.substr(pos, end == std::wstring_view::npos
                                                   ? std::wstring_view::npos
                                                   : end - pos));
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR command_line_ptr, int) {
    auto result = nopeui::Window::Create(900, 600, u"NopeUI Window Smoke");
    if (!result.ok()) {
        return 1;
    }

    std::wstring_view command_line =
        command_line_ptr != nullptr ? std::wstring_view(command_line_ptr) : std::wstring_view{};
    auto window = std::move(result).value();

    auto page = nopeui::Border::Create();
    page->SetBorderVisible(false);
    page->SetPadding(nopeui::Thickness{32.0f, 32.0f, 32.0f, 32.0f});

    auto card = nopeui::Border::Create();
    card->UseSurfaceBackground(true);
    card->SetCornerRadius(8.0f);
    card->SetPadding(nopeui::Thickness{28.0f, 24.0f, 28.0f, 24.0f});

    auto panel = nopeui::StackPanel::Create(14.0f);
    panel->Add(nopeui::Label::Create(L"NopeUI"));
    panel->Add(nopeui::Label::Create(L"P0 - layout and controls"));
    panel->Add(nopeui::Label::Create(L"Tab moves focus; Enter or Space activates."));

    auto light_button = nopeui::Button::Create(L"Light theme");
    auto dark_button = nopeui::Button::Create(L"Dark theme");
    auto* window_ptr = window.Get();
    light_button->SetOnClick([window_ptr] {
        window_ptr->SetTheme(nopeui::LightTheme());
    });
    dark_button->SetOnClick([window_ptr] {
        window_ptr->SetTheme(nopeui::DarkTheme());
    });

    auto theme_row = nopeui::StackPanel::Create(10.0f);
    theme_row->SetOrientation(nopeui::Orientation::Horizontal);
    theme_row->Add(light_button);
    theme_row->Add(dark_button);
    auto disabled_button = nopeui::Button::Create(L"Disabled");
    disabled_button->SetEnabled(false);
    theme_row->Add(disabled_button);
    panel->Add(theme_row);

    auto options_row = nopeui::StackPanel::Create(16.0f);
    options_row->SetOrientation(nopeui::Orientation::Horizontal);
    auto check = nopeui::CheckBox::Create(L"Check box");
    check->SetChecked(true);
    auto toggle = nopeui::ToggleButton::Create(L"Toggle button");
    toggle->SetChecked(true);
    options_row->Add(check);
    options_row->Add(toggle);
    panel->Add(options_row);

    auto radio_row = nopeui::StackPanel::Create(16.0f);
    radio_row->SetOrientation(nopeui::Orientation::Horizontal);
    auto radio_a = nopeui::RadioButton::Create(L"Radio A");
    auto radio_b = nopeui::RadioButton::Create(L"Radio B");
    auto radio_c = nopeui::RadioButton::Create(L"Radio C");
    radio_a->SetChecked(true);
    radio_row->Add(radio_a);
    radio_row->Add(radio_b);
    radio_row->Add(radio_c);
    panel->Add(radio_row);

    auto switch_control = nopeui::ToggleSwitch::Create(L"Toggle switch");
    switch_control->SetChecked(true);
    panel->Add(switch_control);

    auto progress = nopeui::ProgressBar::Create(0.45f);
    progress->SetAccessibilityName(L"Progress");
    panel->Add(progress);

    auto volume = nopeui::Slider::Create(40.0, 0.0, 100.0);
    volume->SetAccessibilityName(L"Volume");
    panel->Add(volume);

    auto photo = nopeui::Image::Create();
    photo->SetImage(MakeSampleImage());
    photo->SetDisplaySize(nopeui::Size{320.0f, 180.0f});
    photo->SetStretch(nopeui::ImageStretch::UniformToFill);
    photo->SetAccessibilityName(L"Sample bitmap");
    panel->Add(photo);

    auto primary = nopeui::Button::Create(L"Templated action");
    primary->SetKey(L"primary");
    panel->Add(primary);

    auto flyout_content = nopeui::StackPanel::Create(10.0f);
    flyout_content->Add(nopeui::Label::Create(L"Anchored flyout"));
    flyout_content->Add(nopeui::Label::Create(L"Escape or an outside click closes it."));
    auto close_flyout = nopeui::Button::Create(L"Close");
    close_flyout->SetOnClick([window_ptr] {
        window_ptr->ClosePopup();
    });
    flyout_content->Add(close_flyout);
    auto flyout = nopeui::Flyout::Create();
    flyout->SetContent(flyout_content);

    auto flyout_button = nopeui::Button::Create(L"Open flyout");
    auto* flyout_anchor = flyout_button.Get();
    flyout_button->SetOnClick([window_ptr, flyout, flyout_anchor] {
        const nopeui::Rect bounds = flyout_anchor->Bounds();
        window_ptr->ShowPopup(
            flyout,
            nopeui::Point{bounds.x, bounds.Bottom() + 6.0f});
    });
    nopeui::ToolTipService::SetToolTip(
        flyout_button.Get(), L"Shows an anchored panel above the content");

    auto popup_row = nopeui::StackPanel::Create(12.0f);
    popup_row->SetOrientation(nopeui::Orientation::Horizontal);
    popup_row->Add(flyout_button);
    panel->Add(popup_row);

    auto menu_target = nopeui::Border::Create();
    menu_target->UseSurfaceBackground(true);
    menu_target->SetCornerRadius(4.0f);
    menu_target->SetPadding(nopeui::Thickness{12.0f, 10.0f, 12.0f, 10.0f});
    menu_target->SetChild(nopeui::Label::Create(
        L"Right-click this surface for a context menu with three actions"));
    auto menu = nopeui::ContextMenu::Create();
    menu->AddItem(L"Copy", [] {});
    menu->AddItem(L"Rename", [] {});
    menu->AddItem(L"Remove", [] {});
    menu_target->SetContextMenu(menu);
    panel->Add(menu_target);

    auto input = nopeui::TextBox::Create(L"Editable single line");
    input->SetPlaceholder(L"Type here and press Enter");
    panel->Add(input);

    panel->Add(nopeui::Label::Create(L"Wheel over the card scrolls the list."));

    nopeui::graphics::TextStyle body;
    body.wrap = true;
    panel->Add(nopeui::Label::Create(
        L"This label wraps against the card width instead of a fixed line, so "
        L"long explanatory text grows downwards and stays inside the layout. "
        L"Resize the window and the line breaks follow the available width "
        L"without rebuilding the whole tree.",
        body));

    nopeui::graphics::TextStyle single;
    single.ellipsis = true;
    panel->Add(nopeui::Label::Create(
        L"This single line is long enough to be trimmed with an ellipsis "
        L"instead of clipping in the middle of a word when the card is narrow. "
        L"The trimmed tail ends with a visible ellipsis when space runs out.",
        single));

    nopeui::graphics::TextStyle mono;
    mono.font_family = L"Consolas";
    panel->Add(nopeui::Label::Create(
        L"Consolas regular: const int value = 42;",
        mono));

    nopeui::graphics::TextStyle centered;
    centered.alignment = nopeui::graphics::TextAlignment::Center;
    panel->Add(nopeui::Label::Create(L"Centered text inside the card", centered));

    nopeui::graphics::TextStyle spaced;
    spaced.wrap = true;
    spaced.line_spacing = 1.4f;
    panel->Add(nopeui::Label::Create(
        L"Line spacing is a multiplier over the font size, so this wrapped\n"
        L"paragraph keeps its natural width handling while the lines sit\n"
        L"further apart and stay readable.",
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

    for (int index = 0; index < 40; ++index) {
        wchar_t row[64]{};
        std::swprintf(row, std::size(row), L"Scrollable row %02d", index);
        panel->Add(nopeui::Label::Create(row));
    }

    auto scroll = nopeui::ScrollViewer::Create();
    scroll->SetChild(panel);
    card->SetChild(scroll);
    page->SetChild(card);
    window->SetContent(page);

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
    window->SetStyleSheet(sheet);

    if (command_line.find(L"--light") != std::wstring_view::npos) {
        window->SetTheme(nopeui::LightTheme());
    }
    if (command_line.find(L"--dark") != std::wstring_view::npos) {
        window->SetTheme(nopeui::DarkTheme());
    }

    window->Show();
    if (command_line.find(L"--sync") != std::wstring_view::npos) {
        window->SetAsyncCommit(false);
        window->RenderFrame();
    }
    if (command_line.find(L"--software") != std::wstring_view::npos) {
        window->SetSoftwareRender(true);
    }

    const auto trace_path = FindTracePath(command_line);
    std::FILE* trace = nullptr;
    if (!trace_path.empty()) {
        std::FILE* opened = nullptr;
        _wfopen_s(&opened, trace_path.c_str(), L"w");
        trace = opened;
        if (trace == nullptr) {
            std::fwprintf(stderr, L"trace open failed: %s\n", trace_path.c_str());
            return 1;
        }
        std::setvbuf(trace, nullptr, _IONBF, 0);
        std::fprintf(
            trace,
            "time_ms,frames,commits,latency_sequence,queue_ms,commit_ms,present_ms,"
            "submitted,completed,dropped,waits,max_depth\n");
    }

    std::atomic<bool> stop_trace{false};
    std::thread trace_thread;
    if (trace != nullptr) {
        trace_thread = std::thread([&] {
            const auto start = std::chrono::steady_clock::now();
            std::uint64_t last_frames = ~0ULL;
            std::uint64_t last_commits = ~0ULL;
            std::uint64_t last_latency_sequence = ~0ULL;
            while (!stop_trace.load(std::memory_order_relaxed)) {
                Sleep(4);
                const auto frames = window->FrameCount();
                const auto commits = window->CommitCount();
                const auto latency = window->LastPresentedFrame();
                const auto stats = window->AsyncCommitStats();
                if (frames == last_frames && commits == last_commits &&
                    latency.sequence == last_latency_sequence) {
                    continue;
                }
                last_frames = frames;
                last_commits = commits;
                last_latency_sequence = latency.sequence;
                const double elapsed_ms =
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start).count();
                std::fprintf(
                    trace,
                    "%.2f,%llu,%llu,%llu,%.3f,%.3f,%.3f,%llu,%llu,%llu,%llu,%u\n",
                    elapsed_ms,
                    static_cast<unsigned long long>(frames),
                    static_cast<unsigned long long>(commits),
                    static_cast<unsigned long long>(latency.sequence),
                    latency.queue_ms,
                    latency.commit_ms,
                    latency.present_ms,
                    static_cast<unsigned long long>(stats.submitted),
                    static_cast<unsigned long long>(stats.completed),
                    static_cast<unsigned long long>(stats.dropped),
                    static_cast<unsigned long long>(stats.waits),
                    stats.max_depth);
            }
        });
    }

    wchar_t title[128]{};
    std::swprintf(
        title,
        std::size(title),
        L"NopeUI Window Smoke - last frame %.2f ms",
        window->LastFrameMilliseconds());
    SetWindowTextW(window->Handle(), title);

    auto loop = nopeui::RunMessageLoop();

    stop_trace.store(true, std::memory_order_relaxed);
    if (trace_thread.joinable()) {
        trace_thread.join();
    }
    if (trace != nullptr) {
        std::fclose(trace);
    }
    if (!loop.ok()) {
        return 1;
    }
    return loop.value();
}
