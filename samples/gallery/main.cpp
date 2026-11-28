#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Windows.h>

#include "nopeui/base/geometry.h"
#include "nopeui/base/property_value.h"
#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/canvas.h"
#include "nopeui/controls/check_box.h"
#include "nopeui/controls/context_menu.h"
#include "nopeui/controls/dock_panel.h"
#include "nopeui/controls/flex_panel.h"
#include "nopeui/controls/flyout.h"
#include "nopeui/controls/grid.h"
#include "nopeui/controls/image.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/menu_bar.h"
#include "nopeui/controls/progress_bar.h"
#include "nopeui/controls/radio_button.h"
#include "nopeui/controls/scroll_viewer.h"
#include "nopeui/controls/slider.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/controls/text_box.h"
#include "nopeui/controls/toggle_button.h"
#include "nopeui/controls/toggle_switch.h"
#include "nopeui/controls/tooltip.h"
#include "nopeui/graphics/image.h"
#include "nopeui/graphics/text.h"
#include "nopeui/layout/attached.h"
#include "nopeui/layout/layout_types.h"
#include "nopeui/platform/window.h"
#include "nopeui/theme/theme.h"

namespace {

using namespace nopeui;

graphics::TextStyle HeadingStyle() {
    graphics::TextStyle style;
    style.size = 16.0f;
    style.bold = true;
    return style;
}

graphics::TextStyle NoteStyle() {
    graphics::TextStyle style;
    style.size = 12.0f;
    style.wrap = true;
    return style;
}

Ref<Border> Card(std::wstring title, Ref<Element> content) {
    auto card = Border::Create();
    card->UseSurfaceBackground(true);
    card->SetCornerRadius(8.0f);
    card->SetPadding(Thickness{16.0f, 14.0f, 16.0f, 14.0f});

    auto stack = StackPanel::Create(10.0f);
    stack->Add(Label::Create(std::move(title), HeadingStyle()));
    stack->Add(std::move(content));
    card->SetChild(stack);
    return card;
}

Ref<Element> SectionBasics() {
    auto column = StackPanel::Create(10.0f);

    auto row = FlexPanel::Create(8.0f);
    row->SetWrap(true);
    auto primary = Button::Create(L"Button");
    primary->SetKey(L"primary");
    row->Add(primary);
    row->Add(Button::Create(L"Secondary"));
    auto toggle = ToggleButton::Create(L"ToggleButton");
    toggle->SetChecked(true);
    row->Add(toggle);
    row->Add(CheckBox::Create(L"CheckBox"));
    auto radio_a = RadioButton::Create(L"Radio A");
    radio_a->SetChecked(true);
    row->Add(radio_a);
    row->Add(RadioButton::Create(L"Radio B"));
    row->Add(ToggleSwitch::Create(L"ToggleSwitch"));
    column->Add(row);

    auto fields = FlexPanel::Create(8.0f);
    fields->SetDirection(FlexDirection::Column);
    auto name_label = Label::Create(L"TextBox");
    name_label->SetMinWidth(90.0f);
    fields->Add(name_label);
    fields->Add(TextBox::Create(L"Type here, Tab moves on"));
    auto long_label = Label::Create(L"TextBox (long text scrolls)");
    long_label->SetMinWidth(90.0f);
    fields->Add(long_label);
    fields->Add(TextBox::Create(
        L"A line long enough that the caret has to scroll the text into view "
        L"while it is edited with the keyboard."));
    column->Add(fields);

    auto ranges = FlexPanel::Create(8.0f);
    ranges->SetDirection(FlexDirection::Column);
    ranges->Add(Label::Create(L"Slider 60 / 0 - 100"));
    ranges->Add(Slider::Create(60.0, 0.0, 100.0));
    ranges->Add(Label::Create(L"ProgressBar 0.72"));
    ranges->Add(ProgressBar::Create(0.72f));
    ranges->Add(Label::Create(L"ProgressBar indeterminate"));
    auto busy = ProgressBar::Create(0.0f);
    busy->SetIndeterminate(true);
    ranges->Add(busy);
    column->Add(ranges);

    auto media = FlexPanel::Create(12.0f);
    media->SetDirection(FlexDirection::Column);

    graphics::Image checker;
    checker.width = 32;
    checker.height = 32;
    checker.pixels.resize(32u * 32u);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const bool dark = ((x / 8) + (y / 8)) % 2 == 0;
            checker.pixels[static_cast<std::size_t>(y) * 32u +
                           static_cast<std::size_t>(x)] = dark ? 0xFF2B2B2Bu : 0xFFD8D8D8u;
        }
    }
    auto picture = Image::Create();
    picture->SetImage(std::move(checker));
    picture->SetDisplaySize(Size{160.0f, 160.0f});
    picture->SetStretch(ImageStretch::Uniform);
    media->Add(picture);
    media->Add(Label::Create(L"Image: checkerboard BGRA, Uniform stretch", NoteStyle()));
    column->Add(media);

    ToolTipService::SetToolTip(primary.Get(), L"Primary button tooltip");
    ToolTipService::SetToolTip(toggle.Get(), L"ToggleButton tooltip");

    auto text = Label::Create(
        L"Tab walks every focusable control in visual order; Space and Enter press "
        L"buttons and toggles, arrows move the slider, and Esc closes popups.",
        NoteStyle());
    column->Add(text);
    return column;
}

Ref<Element> SectionPanels() {
    auto column = StackPanel::Create(10.0f);

    auto grid = Grid::Create();
    grid->SetColumns({AutoLength(), StarLength(1.0f), StarLength(1.0f)});
    grid->SetRows({AutoLength(), AutoLength()});
    grid->SetColumnSpacing(8.0f);
    grid->SetRowSpacing(8.0f);
    auto place = [](Element& element, int row, int column_index) {
        element.SetAttachedProperty(GridRowProperty(), PropertyValue(row));
        element.SetAttachedProperty(GridColumnProperty(), PropertyValue(column_index));
    };
    auto cell_a = Label::Create(L"Grid cell 0,0");
    place(*cell_a, 0, 0);
    auto cell_b = Label::Create(L"star 1");
    place(*cell_b, 0, 1);
    auto cell_c = Label::Create(L"star 2");
    place(*cell_c, 0, 2);
    auto cell_d = Label::Create(L"row 1 spans the stars");
    place(*cell_d, 1, 1);
    cell_d->SetAttachedProperty(GridColumnSpanProperty(), PropertyValue(2));
    grid->Add(cell_a);
    grid->Add(cell_b);
    grid->Add(cell_c);
    grid->Add(cell_d);
    column->Add(grid);

    auto canvas = Canvas::Create();
    canvas->SetHeight(110.0f);
    auto dot_a = Label::Create(L"Canvas at 12, 10");
    dot_a->SetAttachedProperty(CanvasLeftProperty(), PropertyValue(12.0f));
    dot_a->SetAttachedProperty(CanvasTopProperty(), PropertyValue(10.0f));
    auto dot_b = Button::Create(L"Canvas at 40, 55");
    dot_b->SetAttachedProperty(CanvasLeftProperty(), PropertyValue(40.0f));
    dot_b->SetAttachedProperty(CanvasTopProperty(), PropertyValue(55.0f));
    canvas->Add(dot_a);
    canvas->Add(dot_b);
    column->Add(Label::Create(L"Canvas: absolute positions, 110 DIP tall"));
    column->Add(canvas);

    auto dock = DockPanel::Create();
    dock->SetHeight(120.0f);
    dock->SetLastChildFill(true);
    auto dock_item = [](std::wstring text, int edge) {
        auto strip = Border::Create();
        strip->UseSurfaceBackground(true);
        strip->SetPadding(Thickness{10.0f, 6.0f, 10.0f, 6.0f});
        strip->SetChild(Label::Create(std::move(text)));
        strip->SetAttachedProperty(DockProperty(), PropertyValue(edge));
        return strip;
    };
    dock->Add(dock_item(L"Dock Left", 0));
    dock->Add(dock_item(L"Dock Top", 1));
    dock->Add(dock_item(L"Dock Right", 2));
    dock->Add(dock_item(L"Dock Bottom", 3));
    auto fill = Border::Create();
    fill->UseSurfaceBackground(true);
    fill->SetChild(Label::Create(L"LastChildFill"));
    dock->Add(fill);
    column->Add(dock);

    auto flex = FlexPanel::Create(8.0f);
    flex->SetWrap(true);
    for (const wchar_t* text : {L"Flex 1", L"Flex 2", L"Flex 3", L"Flex 4"}) {
        flex->Add(Button::Create(text));
    }
    auto grow = Button::Create(L"FlexGrow 1");
    grow->SetAttachedProperty(FlexGrowProperty(), PropertyValue(1.0f));
    flex->Add(grow);
    column->Add(flex);

    column->Add(Label::Create(
        L"Resize the window: Grid stars and FlexGrow share the slack, DockPanel keeps "
        L"its edges, ScrollViewer grows a scrollbar once the cards no longer fit.",
        NoteStyle()));
    return column;
}

Ref<Element> SectionPopups(Window* window) {
    auto column = StackPanel::Create(10.0f);
    auto row = FlexPanel::Create(8.0f);
    row->SetWrap(true);

    auto hint = Button::Create(L"Hover for tooltip");
    ToolTipService::SetToolTip(hint.Get(), L"ToolTipService shows this after 600 ms");
    row->Add(hint);

    auto flyout = Flyout::Create();
    auto flyout_body = Border::Create();
    flyout_body->UseSurfaceBackground(true);
    flyout_body->SetCornerRadius(8.0f);
    flyout_body->SetPadding(Thickness{16.0f, 12.0f, 16.0f, 12.0f});
    auto flyout_stack = StackPanel::Create(6.0f);
    flyout_stack->Add(Label::Create(L"Flyout content", HeadingStyle()));
    flyout_stack->Add(Label::Create(L"Esc or an outside click closes it.", NoteStyle()));
    flyout_body->SetChild(flyout_stack);
    flyout->SetContent(flyout_body);

    auto open_flyout = Button::Create(L"Open flyout");
    Ref<Element> flyout_ref = flyout;
    open_flyout->SetOnClick([window, flyout_ref] {
        window->ShowPopup(flyout_ref, Point{320.0f, 120.0f});
    });
    row->Add(open_flyout);
    column->Add(row);

    auto menu_host = Border::Create();
    menu_host->UseSurfaceBackground(true);
    menu_host->SetCornerRadius(6.0f);
    menu_host->SetPadding(Thickness{16.0f, 12.0f, 16.0f, 12.0f});
    menu_host->SetChild(Label::Create(L"Right click this card for a ContextMenu"));
    auto menu = ContextMenu::Create();
    menu->AddItem(L"Light theme", [window] { window->SetTheme(LightTheme()); });
    menu->AddItem(L"Dark theme", [window] { window->SetTheme(DarkTheme()); });
    menu->AddItem(L"Contrast theme", [window] { window->SetTheme(HighContrastTheme()); });
    menu_host->SetContextMenu(menu);
    column->Add(menu_host);

    column->Add(Label::Create(
        L"MenuBar above belongs to the window popup layer too: the drop-down is not part "
        L"of the content tree, and Escape or an outside click closes it.",
        NoteStyle()));
    return column;
}

Ref<Element> SectionTheme(Window* window) {
    auto row = FlexPanel::Create(8.0f);
    row->SetWrap(true);
    auto light = Button::Create(L"Light");
    light->SetOnClick([window] { window->SetTheme(LightTheme()); });
    auto dark = Button::Create(L"Dark");
    dark->SetOnClick([window] { window->SetTheme(DarkTheme()); });
    auto contrast = Button::Create(L"Contrast");
    contrast->SetOnClick([window] { window->SetTheme(HighContrastTheme()); });
    auto system = Button::Create(L"System");
    system->SetOnClick([window] { window->SetTheme(SystemTheme()); });
    row->Add(light);
    row->Add(dark);
    row->Add(contrast);
    row->Add(system);
    return row;
}

Ref<Element> BuildGallery(Window* window) {
    auto page = Border::Create();
    page->SetBorderVisible(false);
    page->SetPadding(Thickness{16.0f, 12.0f, 16.0f, 16.0f});

    auto body = DockPanel::Create();
    body->SetLastChildFill(true);

    auto menu = MenuBar::Create();
    menu->AddMenu(L"File", {
        MenuCommand{L"Close", [window] { window->Close(); }},
    });
    menu->AddMenu(L"Edit", {
        MenuCommand{L"Undo", {}},
        MenuCommand{L"Redo", {}},
    });
    menu->AddMenu(L"View", {
        MenuCommand{L"Light theme", [window] { window->SetTheme(LightTheme()); }},
        MenuCommand{L"Dark theme", [window] { window->SetTheme(DarkTheme()); }},
        MenuCommand{L"Contrast theme", [window] { window->SetTheme(HighContrastTheme()); }},
    });
    menu->SetAttachedProperty(DockProperty(), PropertyValue(1));
    body->Add(menu);

    auto scroll = ScrollViewer::Create();
    auto stack = StackPanel::Create(14.0f);
    stack->Add(Label::Create(
        L"Gallery - controls, layout panels, popups and themes in one page", HeadingStyle()));
    stack->Add(Card(L"Controls", SectionBasics()));
    stack->Add(Card(L"Layout panels", SectionPanels()));
    stack->Add(Card(L"Popups and accessibility", SectionPopups(window)));
    stack->Add(Card(L"Theme", SectionTheme(window)));
    scroll->SetChild(stack);
    body->Add(scroll);

    page->SetChild(body);
    return page;
}

}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR command_line_ptr, _In_ int) {
    auto result = Window::Create(1100, 760, u"NopeUI Gallery");
    if (!result.ok()) {
        return 1;
    }

    auto window = std::move(result).value();
    auto* window_ptr = window.Get();

    window->SetContent(BuildGallery(window_ptr));

    const std::wstring_view command_line =
        command_line_ptr != nullptr ? std::wstring_view(command_line_ptr) : std::wstring_view{};
    if (command_line.find(L"--light") != std::wstring_view::npos) {
        window->SetTheme(LightTheme());
    }
    if (command_line.find(L"--dark") != std::wstring_view::npos) {
        window->SetTheme(DarkTheme());
    }
    if (command_line.find(L"--contrast") != std::wstring_view::npos) {
        window->SetTheme(HighContrastTheme());
    }

    window->Show();
    auto loop = RunMessageLoop();
    if (!loop.ok()) {
        return 1;
    }
    return loop.value();
}

