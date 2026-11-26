#include <Windows.h>

#include <string>
#include <string_view>

#include "nopeui/base/strings.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/flyout.h"
#include "nopeui/controls/label.h"
#include "nopeui/markup/xaml.h"
#include "nopeui/platform/window.h"

namespace {

constexpr std::wstring_view kPage = LR"XAML(
<Border Padding="24" SurfaceBackground="True" CornerRadius="10">
  <StackPanel Spacing="12">
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
    <Label Text="XAML smoke page"/>
    <Label Text="This tree was built from XAML, including style and event bindings."/>
    <Button x:Name="show" x:Key="primary" Text="Show flyout"
            ToolTip="Loaded from an attribute" Click="ShowFlyout">
      <Button.ContextMenu>
        <ContextMenu>
          <Item Text="Copy" Click="Copy"/>
          <Item Text="Remove" Click="Remove"/>
        </ContextMenu>
      </Button.ContextMenu>
    </Button>
    <CheckBox x:Name="remember" Label="Remember this page"
              IsChecked="True" CheckedChanged="RememberChanged"/>
    <TextBox x:Name="input" Text="Type here" Placeholder="Enter text"/>
  </StackPanel>
</Border>
)XAML";

std::wstring ErrorText(const nopeui::Error& error) {
    auto converted = nopeui::Utf8ToUtf16(error.message);
    if (!converted.ok()) {
        return L"XAML load failed";
    }
    return std::wstring(converted.value().begin(), converted.value().end());
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    auto created = nopeui::Window::Create(720, 480, u"NopeUI XAML Smoke");
    if (!created.ok()) {
        return 1;
    }
    auto window = std::move(created).value();
    nopeui::Window* window_ptr = window.Get();

    nopeui::xaml::LoadContext context;
    context.resolve_event = [window_ptr](std::wstring_view name) {
        const std::wstring handler_name(name);
        return [window_ptr, handler_name](const nopeui::xaml::EventArgs& args) {
            if (args.event == L"Click" && handler_name == L"ShowFlyout" &&
                args.sender != nullptr) {
                auto content = nopeui::Label::Create(L"Flyout content from XAML");
                auto flyout = nopeui::Flyout::Create();
                flyout->SetContent(content);
                const auto bounds = args.sender->Bounds();
                window_ptr->ShowPopup(
                    flyout,
                    nopeui::Point{bounds.x, bounds.Bottom() + 6.0f});
            }
        };
    };

    auto loaded = nopeui::xaml::Load(kPage, context);
    if (!loaded.ok()) {
        MessageBoxW(
            nullptr,
            ErrorText(loaded.error()).c_str(),
            L"NopeUI XAML Smoke",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    auto page = std::move(loaded).value();
    window->SetStyleSheet(page.style_sheet);
    window->SetContent(page.root);
    window->Show();
    auto loop = nopeui::RunMessageLoop();
    return loop.ok() ? loop.value() : 1;
}
