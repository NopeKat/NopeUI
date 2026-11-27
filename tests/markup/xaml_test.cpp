#include "../test_check.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/canvas.h"
#include "nopeui/controls/check_box.h"
#include "nopeui/controls/content_presenter.h"
#include "nopeui/controls/context_menu.h"
#include "nopeui/controls/dock_panel.h"
#include "nopeui/controls/flex_panel.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/menu_bar.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/controls/text_box.h"
#include "nopeui/layout/style.h"
#include "nopeui/markup/xaml.h"
#include "nopeui/theme/theme.h"

namespace {

bool near(float left, float right) {
    return std::fabs(left - right) < 0.5f;
}

nopeui::xaml::LoadResult LoadPage(std::vector<std::wstring>* events) {
    auto source = LR"XAML(
<StackPanel xmlns="https://nopeui.local/xaml" xmlns:x="https://nopeui.local/xaml"
            Spacing="8">
  <StackPanel.Resources>
    <ControlTemplate x:Key="primaryTemplate" TargetType="Button">
      <Part Name="Background" Background="#0067C0" Border="#005A9E" CornerRadius="6"/>
      <Part Name="Content" Foreground="#FFFFFF"/>
    </ControlTemplate>
    <Style x:Key="primary" TargetType="Button">
      <Setter Property="CornerRadius" Value="6"/>
      <Setter Property="Template" Value="{StaticResource primaryTemplate}"/>
    </Style>
  </StackPanel.Resources>
  <Label x:Name="title" Text="Hello &amp; world"/>
  <Button x:Name="save" x:Key="primary" Text="Save" Click="Save_Click"/>
  <Button x:Name="local" x:Key="primary" Text="Local" CornerRadius="3"/>
  <CheckBox x:Name="remember" Label="Remember" IsChecked="True"
            CheckedChanged="Remember_Changed"/>
  <TextBox x:Name="input" Text="abc" Placeholder="Type here" Submit="Submit_Input"/>
  <ContextMenu x:Name="menu">
    <ContextMenu.Items>
      <Item Text="Copy" Click="Copy_Click"/>
    </ContextMenu.Items>
  </ContextMenu>
  <Border x:Name="menuHost">
    <Border.ContextMenu>
      <ContextMenu>
        <Item Text="Rename" Click="Rename_Click"/>
      </ContextMenu>
    </Border.ContextMenu>
  </Border>
</StackPanel>
)XAML";
    nopeui::xaml::LoadContext context;
    context.resolve_event = [events](std::wstring_view name) -> nopeui::xaml::EventHandler {
        return [events, event = std::wstring(name)](
                   const nopeui::xaml::EventArgs& args) {
            events->push_back(std::wstring(args.event) + L":" +
                              std::wstring(args.handler));
        };
    };
    auto result = nopeui::xaml::Load(source, context);
    if (!result.ok()) {
        std::cerr << result.error().message << "\n";
        return {};
    }
    return std::move(result).value();
}

bool CheckElementCornerRadius(nopeui::Element* element, float expected) {
    if (element == nullptr) {
        std::cerr << "missing element\n";
        return false;
    }
    nopeui::PropertyValue value;
    if (!element->GetProperty(L"CornerRadius", &value)) {
        std::cerr << "missing CornerRadius\n";
        return false;
    }
    float actual = 0.0f;
    if (!value.AsFloat(&actual) || actual != expected) {
        std::cerr << "CornerRadius " << actual << " expected " << expected << "\n";
        return false;
    }
    return true;
}

bool CheckCornerRadius(
    const nopeui::xaml::LoadResult& page,
    std::wstring_view name,
    float expected) {
    return CheckElementCornerRadius(page.Find(name), expected);
}

}  // namespace

int main() {
    auto parsed = nopeui::xaml::Parse(
        L"<?xml version=\"1.0\"?><!-- leading --><Root A='one'>"
        L"<Child/> text &amp; &#x41;</Root>");
    if (!parsed.ok()) {
        std::cerr << "parse: " << parsed.error().message << "\n";
        return 1;
    }
    NOPEUI_CHECK(parsed.ok());
    const auto& root = parsed.value();
    NOPEUI_CHECK(root.name == L"Root");
    NOPEUI_CHECK(root.attributes.size() == 1);
    NOPEUI_CHECK(root.attributes[0].name == L"A");
    NOPEUI_CHECK(root.attributes[0].value == L"one");
    NOPEUI_CHECK(root.children.size() == 1);
    NOPEUI_CHECK(root.children[0].name == L"Child");
    NOPEUI_CHECK(root.text.find(L"text & A") != std::wstring::npos);

    NOPEUI_CHECK(!nopeui::xaml::Parse(L"<Root><Child></Root>").ok());
    NOPEUI_CHECK(!nopeui::xaml::Parse(L"<Root A=\"&bogus;\"/>").ok());

    std::vector<std::wstring> events;
    auto page = LoadPage(&events);
    NOPEUI_CHECK(static_cast<bool>(page.root));
    NOPEUI_CHECK(page.Find(L"title") != nullptr);
    NOPEUI_CHECK(page.Find(L"save") != nullptr);
    NOPEUI_CHECK(page.Find(L"missing") == nullptr);

    auto* panel = dynamic_cast<nopeui::StackPanel*>(page.root.Get());
    NOPEUI_CHECK(panel != nullptr);
    nopeui::PropertyValue spacing;
    NOPEUI_CHECK(page.root->GetProperty(L"Spacing", &spacing));
    float spacing_value = 0.0f;
    NOPEUI_CHECK(spacing.AsFloat(&spacing_value) && spacing_value == 8.0f);

    nopeui::PropertyValue title;
    NOPEUI_CHECK(page.Find(L"title")->GetProperty(L"Text", &title));
    std::wstring title_text;
    NOPEUI_CHECK(title.AsString(&title_text));
    NOPEUI_CHECK(title_text == L"Hello & world");

    nopeui::PropertyValue radius;
    NOPEUI_CHECK(page.Find(L"save")->GetProperty(L"CornerRadius", &radius));
    float save_radius = 0.0f;
    NOPEUI_CHECK(radius.AsFloat(&save_radius) && save_radius == 6.0f);

    NOPEUI_CHECK(page.Find(L"local")->GetProperty(L"CornerRadius", &radius));
    float local_radius = 0.0f;
    NOPEUI_CHECK(radius.AsFloat(&local_radius) && local_radius == 3.0f);

    auto* save = dynamic_cast<nopeui::Button*>(page.Find(L"save"));
    NOPEUI_CHECK(save != nullptr);
    nopeui::Element* save_element = save;
    save_element->OnKeyDown(13);
    save_element->OnKeyUp(13);
    NOPEUI_CHECK(events.size() == 1 && events[0] == L"Click:Save_Click");

    auto* remember = dynamic_cast<nopeui::CheckBox*>(page.Find(L"remember"));
    NOPEUI_CHECK(remember != nullptr);
    remember->SetChecked(false);
    NOPEUI_CHECK(events.size() == 2 && events[1] == L"CheckedChanged:Remember_Changed");

    auto* input = dynamic_cast<nopeui::TextBox*>(page.Find(L"input"));
    NOPEUI_CHECK(input != nullptr);
    nopeui::Element* input_element = input;
    input_element->OnKeyDown(13);
    NOPEUI_CHECK(events.size() == 3 && events[2] == L"Submit:Submit_Input");

    nopeui::PropertyValue placeholder;
    NOPEUI_CHECK(input->GetProperty(L"Placeholder", &placeholder));
    std::wstring placeholder_text;
    NOPEUI_CHECK(placeholder.AsString(&placeholder_text));
    NOPEUI_CHECK(placeholder_text == L"Type here");

    auto* menu_host = dynamic_cast<nopeui::Border*>(page.Find(L"menuHost"));
    NOPEUI_CHECK(menu_host != nullptr);
    NOPEUI_CHECK(dynamic_cast<nopeui::ContextMenu*>(menu_host->GetContextMenu()) != nullptr);

    auto scoped = nopeui::xaml::Load(LR"XAML(
<StackPanel Spacing="8">
  <StackPanel.Resources>
    <Style TargetType="Button">
      <Setter Property="CornerRadius" Value="9"/>
    </Style>
  </StackPanel.Resources>
  <Button x:Name="outer" Text="Outer"/>
  <StackPanel>
    <StackPanel.Resources>
      <Style TargetType="Button">
        <Setter Property="CornerRadius" Value="2"/>
      </Style>
    </StackPanel.Resources>
    <Button x:Name="inner" Text="Inner"/>
  </StackPanel>
  <StackPanel>
    <StackPanel.Resources>
      <Style x:Key="primary" TargetType="Button">
        <Setter Property="CornerRadius" Value="4"/>
      </Style>
    </StackPanel.Resources>
    <Button x:Name="left" x:Key="primary" Text="Left"/>
  </StackPanel>
  <StackPanel>
    <StackPanel.Resources>
      <Style x:Key="primary" TargetType="Button">
        <Setter Property="CornerRadius" Value="7"/>
      </Style>
    </StackPanel.Resources>
    <Button x:Name="right" x:Key="primary" Text="Right"/>
  </StackPanel>
</StackPanel>
)XAML");
    if (!scoped.ok()) {
        std::cerr << scoped.error().message << "\n";
        return 1;
    }
    auto scoped_page = std::move(scoped).value();
    NOPEUI_CHECK(CheckCornerRadius(scoped_page, L"outer", 9.0f));
    NOPEUI_CHECK(CheckCornerRadius(scoped_page, L"inner", 2.0f));
    NOPEUI_CHECK(CheckCornerRadius(scoped_page, L"left", 4.0f));
    NOPEUI_CHECK(CheckCornerRadius(scoped_page, L"right", 7.0f));

    const std::filesystem::path file_path =
        std::filesystem::temp_directory_path() / L"nopeui_xaml_load_file_test.xaml";
    {
        std::ofstream stream(file_path, std::ios::binary);
        stream << "<Label xmlns:x=\"https://nopeui.local/xaml\" "
                  "x:Name=\"fromFile\" Text=\"From file\"/>";
    }
    const auto file_utf8 = file_path.u8string();
    auto from_file = nopeui::xaml::LoadFile(
        std::string(file_utf8.begin(), file_utf8.end()));
    std::filesystem::remove(file_path);
    if (!from_file.ok()) {
        std::cerr << from_file.error().message << "\n";
        return 1;
    }
    auto file_page = std::move(from_file).value();
    nopeui::PropertyValue file_text;
    NOPEUI_CHECK(file_page.Find(L"fromFile") != nullptr);
    NOPEUI_CHECK(file_page.Find(L"fromFile")->GetProperty(L"Text", &file_text));
    std::wstring file_string;
    NOPEUI_CHECK(file_text.AsString(&file_string));
    NOPEUI_CHECK(file_string == L"From file");

    auto visual = nopeui::xaml::Load(LR"XAML(
<StackPanel Spacing="8">
  <StackPanel.Resources>
    <ControlTemplate x:Key="visual" TargetType="Button">
      <Border Background="#0067C0" Padding="12,6"
              CornerRadius="{TemplateBinding CornerRadius}">
        <StackPanel Orientation="Horizontal" Spacing="6">
          <Label Text="*" Foreground="{TemplateBinding Foreground}"/>
          <ContentPresenter Text="{TemplateBinding Text}"
                            Foreground="{TemplateBinding Foreground}"/>
        </StackPanel>
      </Border>
    </ControlTemplate>
    <Style x:Key="visualStyle" TargetType="Button">
      <Setter Property="CornerRadius" Value="9"/>
      <Setter Property="Foreground" Value="#FFFFFF"/>
      <Setter Property="Template" Value="{StaticResource visual}"/>
    </Style>
    <ControlTemplate x:Key="plainVisual" TargetType="Button">
      <Border Background="#333333" Padding="8">
        <ContentPresenter/>
      </Border>
    </ControlTemplate>
    <Style x:Key="plainVisualStyle" TargetType="Button">
      <Setter Property="Template" Value="{StaticResource plainVisual}"/>
    </Style>
  </StackPanel.Resources>
  <Button x:Name="templated" x:Key="visualStyle" Text="Templated"/>
  <Button x:Name="autofill" x:Key="plainVisualStyle" Text="Auto"/>
</StackPanel>
)XAML");
    if (!visual.ok()) {
        std::cerr << visual.error().message << "\n";
        return 1;
    }
    auto visual_page = std::move(visual).value();
    auto* templated = dynamic_cast<nopeui::Button*>(visual_page.Find(L"templated"));
    NOPEUI_CHECK(templated != nullptr);
    NOPEUI_CHECK(templated->TemplateRoot() != nullptr);
    auto* visual_root = dynamic_cast<nopeui::Border*>(templated->TemplateRoot());
    NOPEUI_CHECK(visual_root != nullptr);
    NOPEUI_CHECK(CheckElementCornerRadius(visual_root, 9.0f));

    nopeui::PropertyValue background;
    NOPEUI_CHECK(visual_root->GetProperty(L"Background", &background));
    std::uint32_t background_color = 0;
    NOPEUI_CHECK(background.AsColor(&background_color));
    NOPEUI_CHECK(background_color == 0x0067C0u);

    std::vector<nopeui::Element*> row;
    nopeui::Element* root_element = visual_root;
    root_element->CollectChildren(row);
    NOPEUI_CHECK(row.size() == 1);
    auto* row_panel = dynamic_cast<nopeui::StackPanel*>(row[0]);
    NOPEUI_CHECK(row_panel != nullptr);

    std::vector<nopeui::Element*> items;
    nopeui::Element* row_element = row_panel;
    row_element->CollectChildren(items);
    NOPEUI_CHECK(items.size() == 2);
    auto* presenter = dynamic_cast<nopeui::ContentPresenter*>(items[1]);
    NOPEUI_CHECK(presenter != nullptr);
    NOPEUI_CHECK(presenter->Text() == L"Templated");

    nopeui::PropertyValue foreground;
    NOPEUI_CHECK(presenter->GetProperty(L"Foreground", &foreground));
    std::uint32_t foreground_color = 0;
    NOPEUI_CHECK(foreground.AsColor(&foreground_color));
    NOPEUI_CHECK(foreground_color == 0xFFFFFFu);

    NOPEUI_CHECK(templated->SetProperty(
        L"CornerRadius",
        nopeui::PropertyValue(3.0f)));
    NOPEUI_CHECK(CheckElementCornerRadius(visual_root, 3.0f));
    NOPEUI_CHECK(templated->SetProperty(
        L"Foreground",
        nopeui::PropertyValue(static_cast<std::uint32_t>(0x00FF00))));
    NOPEUI_CHECK(presenter->GetProperty(L"Foreground", &foreground));
    NOPEUI_CHECK(foreground.AsColor(&foreground_color));
    NOPEUI_CHECK(foreground_color == 0x00FF00u);

    auto* autofill = dynamic_cast<nopeui::Button*>(visual_page.Find(L"autofill"));
    NOPEUI_CHECK(autofill != nullptr);
    auto* autofill_root = dynamic_cast<nopeui::Border*>(autofill->TemplateRoot());
    NOPEUI_CHECK(autofill_root != nullptr);
    std::vector<nopeui::Element*> autofill_children;
    nopeui::Element* autofill_element = autofill_root;
    autofill_element->CollectChildren(autofill_children);
    NOPEUI_CHECK(autofill_children.size() == 1);
    auto* autofill_presenter =
        dynamic_cast<nopeui::ContentPresenter*>(autofill_children[0]);
    NOPEUI_CHECK(autofill_presenter != nullptr);
    NOPEUI_CHECK(autofill_presenter->Text() == L"Auto");

    // Canvas and DockPanel take their attached properties from markup.
    {
        auto loaded = nopeui::xaml::Load(LR"XAML(
<Canvas xmlns="https://nopeui.local/xaml">
  <Label x:Name="pin" Canvas.Left="12" Canvas.Top="8" Text="Pinned"/>
</Canvas>)XAML");
        NOPEUI_CHECK(loaded.ok());
        NOPEUI_CHECK(dynamic_cast<nopeui::Canvas*>(loaded.value().root.Get()) != nullptr);
        auto* pin = loaded.value().Find(L"pin");
        NOPEUI_CHECK(pin != nullptr);
        nopeui::PropertyValue left;
        NOPEUI_CHECK(pin->GetAttachedProperty(L"Canvas.Left", &left));
        float offset = 0.0f;
        NOPEUI_CHECK(left.AsFloat(&offset));
        NOPEUI_CHECK(offset == 12.0f);

        NOPEUI_CHECK(!nopeui::xaml::Load(
            L"<Canvas xmlns=\"https://nopeui.local/xaml\">"
            L"<Label Canvas.Left=\"oops\"/></Canvas>").ok());
    }

    {
        auto loaded = nopeui::xaml::Load(LR"XAML(
<DockPanel xmlns="https://nopeui.local/xaml" LastChildFill="False">
  <Label x:Name="docked" Dock="Top" Text="Header"/>
  <Label Text="Filler"/>
</DockPanel>)XAML");
        NOPEUI_CHECK(loaded.ok());
        auto* dock = dynamic_cast<nopeui::DockPanel*>(loaded.value().root.Get());
        NOPEUI_CHECK(dock != nullptr);
        NOPEUI_CHECK(!dock->LastChildFill());
        auto* docked = loaded.value().Find(L"docked");
        NOPEUI_CHECK(docked != nullptr);
        nopeui::PropertyValue side;
        NOPEUI_CHECK(docked->GetAttachedProperty(L"Dock", &side));
        int value = 0;
        NOPEUI_CHECK(side.AsInt(&value));
        NOPEUI_CHECK(value == 1);

        NOPEUI_CHECK(!nopeui::xaml::Load(
            L"<DockPanel xmlns=\"https://nopeui.local/xaml\">"
            L"<Label Dock=\"Diagonal\"/></DockPanel>").ok());
    }

    // A value resource on an ancestor feeds a {ThemeResource} reference.
    {
        auto loaded = nopeui::xaml::Load(LR"XAML(
<Border xmlns="https://nopeui.local/xaml" Padding="8">
  <Border.Resources>
    <Color Key="LocalAccent">#123456</Color>
  </Border.Resources>
  <Label x:Name="tinted" Foreground="{ThemeResource LocalAccent}" Text="Tint"/>
</Border>)XAML");
        NOPEUI_CHECK(loaded.ok());
        auto* tinted = loaded.value().Find(L"tinted");
        NOPEUI_CHECK(tinted != nullptr);
        NOPEUI_CHECK(tinted->Foreground() == 0x123456u);
    }

    // A palette key follows the theme, so light and dark differ.
    {
        auto loaded = nopeui::xaml::Load(LR"XAML(
<Label xmlns="https://nopeui.local/xaml" x:Name="accent"
       Foreground="{ThemeResource Accent}" Text="Accent"/>)XAML");
        NOPEUI_CHECK(loaded.ok());
        auto* accent = loaded.value().Find(L"accent");
        NOPEUI_CHECK(accent != nullptr);
        NOPEUI_CHECK(accent->Foreground() == 0x0067C0u);
        accent->SetTheme(&nopeui::DarkTheme());
        accent->RefreshThemeResources();
        NOPEUI_CHECK(accent->Foreground() == 0x60CDFFu);
        accent->SetTheme(&nopeui::LightTheme());
    }

    // A style trigger keeps writing while the watched property matches.
    {
        auto loaded = nopeui::xaml::Load(LR"XAML(
<StackPanel xmlns="https://nopeui.local/xaml">
  <StackPanel.Resources>
    <Style TargetType="Label">
      <Setter Property="Foreground" Value="#1B1B1B"/>
      <Trigger Property="IsEnabled" Value="False">
        <Setter Property="Foreground" Value="#9A9A9A"/>
      </Trigger>
    </Style>
  </StackPanel.Resources>
  <Label x:Name="dim" Text="Dim"/>
</StackPanel>)XAML");
        NOPEUI_CHECK(loaded.ok());
        auto* dim = loaded.value().Find(L"dim");
        NOPEUI_CHECK(dim != nullptr);
        NOPEUI_CHECK(dim->Foreground() == 0x1B1B1Bu);
        dim->SetEnabled(false);
        NOPEUI_CHECK(dim->Foreground() == 0x9A9A9Au);
        dim->SetEnabled(true);
        NOPEUI_CHECK(dim->Foreground() == 0x1B1B1Bu);
    }

    // A template with visual states parses and still applies its parts.
    {
        auto loaded = nopeui::xaml::Load(LR"XAML(
<StackPanel xmlns="https://nopeui.local/xaml">
  <StackPanel.Resources>
    <ControlTemplate x:Key="stateful" TargetType="Button">
      <Part Name="Background" Background="#0067C0" CornerRadius="4"/>
      <Part Name="Content" Foreground="#FFFFFF"/>
      <VisualStateGroup Name="CommonStates">
        <VisualState Name="PointerOver">
          <StateSetter Part="Background" Background="#005BA8"/>
        </VisualState>
        <VisualState Name="Disabled">
          <StateSetter Part="Background" Background="#F0F0F0"/>
        </VisualState>
      </VisualStateGroup>
    </ControlTemplate>
  </StackPanel.Resources>
  <Button x:Name="stateful" Text="Stateful" Template="{StaticResource stateful}"/>
</StackPanel>)XAML");
        NOPEUI_CHECK(loaded.ok());
        NOPEUI_CHECK(loaded.value().Find(L"stateful") != nullptr);

        NOPEUI_CHECK(!nopeui::xaml::Load(LR"XAML(
<ControlTemplate xmlns="https://nopeui.local/xaml" x:Key="bad" TargetType="Button">
  <VisualStateGroup Name="CommonStates"/>
</ControlTemplate>)XAML").ok());
    }

    // MenuBar markup keeps its titles and opens a menu through the root.
    {
        nopeui::xaml::LoadContext context;
        context.resolve_event = [](std::wstring_view) {
            return nopeui::xaml::EventHandler([](const nopeui::xaml::EventArgs&) {});
        };
        auto loaded = nopeui::xaml::Load(LR"XAML(
<MenuBar xmlns="https://nopeui.local/xaml">
  <Menu Title="File">
    <Item Text="New" Click="New_Click"/>
    <Item Text="Open"/>
  </Menu>
  <Menu Title="Edit">
    <Item Text="Undo"/>
  </Menu>
</MenuBar>)XAML", context);
        NOPEUI_CHECK(loaded.ok());
        auto* bar = dynamic_cast<nopeui::MenuBar*>(loaded.value().root.Get());
        NOPEUI_CHECK(bar != nullptr);
        NOPEUI_CHECK(bar->MenuCount() == 2);

        nopeui::Ref<nopeui::Element> shown;
        nopeui::Point anchor{};
        bar->SetPopupHandler(
            [&shown, &anchor](nopeui::Ref<nopeui::Element> popup, nopeui::Point point) {
                shown = std::move(popup);
                anchor = point;
            },
            [] {});
        NOPEUI_CHECK(static_cast<nopeui::Element*>(bar)->OnKeyDown(0x0D));
        NOPEUI_CHECK(shown.Get() != nullptr);
        NOPEUI_CHECK(dynamic_cast<nopeui::ContextMenu*>(shown.Get()) != nullptr);
        NOPEUI_CHECK(anchor.y >= bar->Bounds().Bottom());
    }

    // FlexPanel markup carries direction, gap, wrap and per-child grow.
    {
        auto loaded = nopeui::xaml::Load(LR"XAML(
<FlexPanel xmlns="https://nopeui.local/xaml" Direction="Column" Gap="4" Wrap="True">
  <Label Text="A" FlexGrow="1"/>
  <Label Text="B"/>
</FlexPanel>)XAML");
        NOPEUI_CHECK(loaded.ok());
        auto* flex = dynamic_cast<nopeui::FlexPanel*>(loaded.value().root.Get());
        NOPEUI_CHECK(flex != nullptr);
        NOPEUI_CHECK(flex->GetDirection() == nopeui::FlexDirection::Column);
        NOPEUI_CHECK(near(flex->Gap(), 4.0f));
        NOPEUI_CHECK(flex->Wrap());
        const auto& children = flex->Children();
        NOPEUI_CHECK(children.size() == 2);
        nopeui::PropertyValue grow;
        NOPEUI_CHECK(children[0]->GetAttachedProperty(L"FlexGrow", &grow));
        float weight = 0.0f;
        NOPEUI_CHECK(grow.AsFloat(&weight) && near(weight, 1.0f));

        NOPEUI_CHECK(!nopeui::xaml::Load(LR"XAML(
<FlexPanel xmlns="https://nopeui.local/xaml" Direction="Diagonal"/>)XAML").ok());
    }

    // A numeric resource drives a float property, resolved after the load.
    {
        auto loaded = nopeui::xaml::Load(LR"XAML(
<Border xmlns="https://nopeui.local/xaml" Padding="8">
  <Border.Resources>
    <Number Key="Radius">6</Number>
  </Border.Resources>
  <Button x:Name="rounded" CornerRadius="{ThemeResource Radius}" Text="Rounded"/>
</Border>)XAML");
        NOPEUI_CHECK(loaded.ok());
        auto* rounded = loaded.value().Find(L"rounded");
        NOPEUI_CHECK(rounded != nullptr);
        nopeui::PropertyValue corner;
        float corner_value = 0.0f;
        NOPEUI_CHECK(rounded->GetProperty(L"CornerRadius", &corner));
        NOPEUI_CHECK(corner.AsFloat(&corner_value) && near(corner_value, 6.0f));

        NOPEUI_CHECK(!nopeui::xaml::Load(LR"XAML(
<Button xmlns="https://nopeui.local/xaml" Text="Wrong"
        CornerRadius="{ThemeResource Accent}"/>)XAML").ok());
    }

    // A typo in a theme resource key is rejected once the tree is complete.
    NOPEUI_CHECK(!nopeui::xaml::Load(LR"XAML(
<Label xmlns="https://nopeui.local/xaml" Foreground="{ThemeResource Accentt}"/>)XAML").ok());

    // Resources only accept the supported value elements.
    NOPEUI_CHECK(!nopeui::xaml::Load(LR"XAML(
<Border xmlns="https://nopeui.local/xaml">
  <Border.Resources>
    <Brush Key="Nope">#123456</Brush>
  </Border.Resources>
</Border>)XAML").ok());

    // Border properties set in markup are local values and outrank a style.
    {
        auto loaded = nopeui::xaml::Load(LR"XAML(
<Border xmlns="https://nopeui.local/xaml" x:Name="local"
        Background="#00FF00" BorderColor="#0000FF"/>)XAML");
        NOPEUI_CHECK(loaded.ok());
        auto* local = dynamic_cast<nopeui::Border*>(
            loaded.value().Find(L"local"));
        NOPEUI_CHECK(local != nullptr);

        auto style = nopeui::Style::Create();
        style->SetTargetType(L"Border");
        style->Set(L"Background", nopeui::PropertyValue(0x00FF0000u));
        style->Set(L"BorderColor", nopeui::PropertyValue(0x00FF0000u));
        style->Apply(*local);

        nopeui::PropertyValue local_background;
        nopeui::PropertyValue local_border;
        std::uint32_t local_background_color = 0;
        std::uint32_t local_border_color = 0;
        NOPEUI_CHECK(local->GetProperty(L"Background", &local_background));
        NOPEUI_CHECK(local->GetProperty(L"BorderColor", &local_border));
        NOPEUI_CHECK(local_background.AsColor(&local_background_color) &&
                     local_background_color == 0x00FF00u);
        NOPEUI_CHECK(local_border.AsColor(&local_border_color) &&
                     local_border_color == 0x0000FFu);
    }

    NOPEUI_CHECK(!nopeui::xaml::Load(L"<Label Unknown=\"value\"/>").ok());
    NOPEUI_CHECK(!nopeui::xaml::Load(L"<Button Text=\"x\" Click=\"Missing\"/>").ok());

    return 0;
}
