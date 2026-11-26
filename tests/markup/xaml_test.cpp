#include "../test_check.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/check_box.h"
#include "nopeui/controls/context_menu.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/controls/text_box.h"
#include "nopeui/markup/xaml.h"

namespace {

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
    NOPEUI_CHECK(static_cast<bool>(page.style_sheet));
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

    NOPEUI_CHECK(!nopeui::xaml::Load(L"<Label Unknown=\"value\"/>").ok());
    NOPEUI_CHECK(!nopeui::xaml::Load(L"<Button Text=\"x\" Click=\"Missing\"/>").ok());

    return 0;
}
