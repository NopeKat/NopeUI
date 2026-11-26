#include "../test_check.h"

#include <string>

#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/layout/style.h"
#include "nopeui/theme/control_template.h"

int main() {
    auto button = nopeui::Button::Create(L"Apply");
    nopeui::PropertyValue value;

    NOPEUI_CHECK(button->GetProperty(L"Text", &value));
    NOPEUI_CHECK(value.Type() == nopeui::PropertyValueType::String);

    NOPEUI_CHECK(!button->SetProperty(L"CornerRadius", nopeui::PropertyValue(2)));
    NOPEUI_CHECK(button->SetProperty(L"CornerRadius", nopeui::PropertyValue(5.0f)));
    NOPEUI_CHECK(button->GetProperty(L"CornerRadius", &value));
    float radius = 0.0f;
    NOPEUI_CHECK(value.AsFloat(&radius) && radius == 5.0f);

    int changes = 0;
    std::wstring changed_name;
    auto subscription = button->SubscribePropertyChanged(
        [&changes, &changed_name](
            const nopeui::PropertyMetadata& metadata,
            const nopeui::PropertyValue&,
            const nopeui::PropertyValue&) {
            ++changes;
            changed_name = metadata.name;
        });
    NOPEUI_CHECK(button->SetProperty(L"CornerRadius", nopeui::PropertyValue(7.0f)));
    NOPEUI_CHECK(changes == 1);
    NOPEUI_CHECK(changed_name == L"CornerRadius");

    NOPEUI_CHECK(!button->SetProperty(
        L"IsEnabled", nopeui::PropertyValue(std::wstring(L"yes"))));
    NOPEUI_CHECK(button->SetProperty(L"IsEnabled", nopeui::PropertyValue(false)));
    NOPEUI_CHECK(!button->IsEnabled());
    NOPEUI_CHECK(changes == 2);
    NOPEUI_CHECK(changed_name == L"IsEnabled");

    auto parent = nopeui::Border::Create();
    auto child = nopeui::Label::Create(L"Child");
    parent->SetChild(child);
    parent->SetForeground(0x00ff0000u);
    NOPEUI_CHECK(child->Foreground() == 0x00ff0000u);
    child->SetForeground(0x0000ff00u);
    NOPEUI_CHECK(child->Foreground() == 0x0000ff00u);

    auto panel = nopeui::StackPanel::Create(4.0f);
    auto primary = nopeui::Button::Create(L"Primary");
    primary->SetKey(L"primary");
    auto plain = nopeui::Button::Create(L"Plain");
    panel->Add(primary);
    panel->Add(plain);

    auto sheet = nopeui::StyleSheet::Create();
    auto style = nopeui::Style::Create();
    style->SetTargetType(L"Button");
    style->SetTargetKey(L"primary");
    style->Set(L"CornerRadius", nopeui::PropertyValue(4.0f));

    auto control_template = nopeui::ControlTemplate::Create();
    nopeui::TemplatePart background;
    background.background = 0x0067c0u;
    background.border = 0x005a9eu;
    background.corner_radius = 8.0f;
    control_template->SetPart(L"Background", background);
    style->SetTemplate(control_template);
    sheet->Add(style);
    sheet->ApplyTo(*panel);

    NOPEUI_CHECK(primary->GetProperty(L"CornerRadius", &value));
    NOPEUI_CHECK(value.AsFloat(&radius) && radius == 8.0f);
    NOPEUI_CHECK(plain->GetProperty(L"CornerRadius", &value));
    NOPEUI_CHECK(value.AsFloat(&radius) && radius == 4.0f);

    return 0;
}
