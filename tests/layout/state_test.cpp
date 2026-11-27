#include "../test_check.h"

#include "nopeui/controls/button.h"
#include "nopeui/controls/check_box.h"
#include "nopeui/controls/label.h"
#include "nopeui/layout/style.h"
#include "nopeui/theme/control_template.h"

namespace {

nopeui::PropertyTrigger DisabledForeground() {
    nopeui::PropertyTrigger trigger;
    trigger.property = L"IsEnabled";
    trigger.value = nopeui::PropertyValue(false);
    trigger.setters.emplace_back(
        L"Foreground", nopeui::PropertyValue(0x9A9A9Au));
    return trigger;
}

}  // namespace

int main() {
    // Visual states patch a part and leave untouched fields alone.
    {
        auto control_template = nopeui::ControlTemplate::Create();
        nopeui::TemplatePart base;
        base.background = 0x0067C0;
        base.corner_radius = 4.0f;
        control_template->SetPart(L"Background", base);

        nopeui::TemplatePart hover;
        hover.background = 0x005BA8;
        control_template->SetStatePart(
            L"CommonStates", L"PointerOver", L"Background", hover);

        auto normal = control_template->ResolvePart(
            L"Background", L"CommonStates", L"Normal");
        auto hovered = control_template->ResolvePart(
            L"Background", L"CommonStates", L"PointerOver");
        auto pressed = control_template->ResolvePart(
            L"Background", L"CommonStates", L"Pressed");

        NOPEUI_CHECK(normal.background == 0x0067C0);
        NOPEUI_CHECK(hovered.background == 0x005BA8);
        NOPEUI_CHECK(hovered.corner_radius == 4.0f);
        NOPEUI_CHECK(pressed.background == 0x0067C0);
        NOPEUI_CHECK(control_template->HasState(L"CommonStates"));
        NOPEUI_CHECK(!control_template->HasState(L"FocusStates"));
    }

    // A button accepts a template with states and keeps it alive.
    {
        auto control_template = nopeui::ControlTemplate::Create();
        nopeui::TemplatePart base;
        base.background = 0x0067C0;
        control_template->SetPart(L"Background", base);
        nopeui::TemplatePart disabled;
        disabled.background = 0xF0F0F0;
        control_template->SetStatePart(
            L"CommonStates", L"Disabled", L"Background", disabled);

        auto button = nopeui::Button::Create(L"Save");
        auto style = nopeui::Style::Create();
        style->SetTargetType(L"Button");
        style->SetTemplate(control_template);
        style->Apply(*button);
        control_template.Reset();
        // The button holds its own reference, so state resolution survives.
        button->SetEnabled(false);
        NOPEUI_CHECK(!button->IsEnabled());
    }

    // A trigger writes above the style value and below a local value.
    {
        auto style = nopeui::Style::Create();
        style->SetTargetType(L"Label");
        style->Set(L"Foreground", nopeui::PropertyValue(0x1B1B1Bu));
        style->AddTrigger(DisabledForeground());

        auto label = nopeui::Label::Create(L"Field");
        style->Apply(*label);
        NOPEUI_CHECK(label->Foreground() == 0x1B1B1B);

        label->SetEnabled(false);
        NOPEUI_CHECK(label->Foreground() == 0x9A9A9A);

        label->SetEnabled(true);
        NOPEUI_CHECK(label->Foreground() == 0x1B1B1B);
    }

    // A local value outranks the trigger, and the trigger leaves it alone.
    {
        auto style = nopeui::Style::Create();
        style->SetTargetType(L"Label");
        style->Set(L"Foreground", nopeui::PropertyValue(0x1B1B1Bu));
        style->AddTrigger(DisabledForeground());

        auto label = nopeui::Label::Create(L"Fixed");
        label->SetProperty(L"Foreground", nopeui::PropertyValue(0x123456u));
        style->Apply(*label);
        label->SetEnabled(false);
        NOPEUI_CHECK(label->Foreground() == 0x123456);
    }

    // Matching triggers run once when the style lands.
    {
        auto style = nopeui::Style::Create();
        style->SetTargetType(L"Label");
        style->Set(L"Foreground", nopeui::PropertyValue(0x1B1B1Bu));
        style->AddTrigger(DisabledForeground());

        auto label = nopeui::Label::Create(L"Already off");
        label->SetEnabled(false);
        style->Apply(*label);
        NOPEUI_CHECK(label->Foreground() == 0x9A9A9A);
    }

    // ClearValue hands the property back to a matching trigger, then the style.
    {
        auto style = nopeui::Style::Create();
        style->SetTargetType(L"Label");
        style->Set(L"Foreground", nopeui::PropertyValue(0x1B1B1Bu));
        style->AddTrigger(DisabledForeground());

        auto label = nopeui::Label::Create(L"Cleared");
        label->SetProperty(L"Foreground", nopeui::PropertyValue(0x123456u));
        style->Apply(*label);
        label->SetEnabled(false);
        NOPEUI_CHECK(label->Foreground() == 0x123456);

        NOPEUI_CHECK(label->ClearValue(L"Foreground"));
        NOPEUI_CHECK(label->Foreground() == 0x9A9A9A);
        label->SetEnabled(true);
        NOPEUI_CHECK(label->Foreground() == 0x1B1B1B);
    }

    // IsChecked changes raise a property notification, so triggers run when a
    // toggle control changes state through the setter or Toggle().
    {
        auto style = nopeui::Style::Create();
        style->SetTargetType(L"CheckBox");
        style->Set(L"Foreground", nopeui::PropertyValue(0x1B1B1Bu));
        nopeui::PropertyTrigger checked;
        checked.property = L"IsChecked";
        checked.value = nopeui::PropertyValue(true);
        checked.setters.emplace_back(
            L"Foreground", nopeui::PropertyValue(0x00FF00u));
        style->AddTrigger(checked);

        auto box = nopeui::CheckBox::Create(L"Toggle me");
        style->Apply(*box);
        NOPEUI_CHECK(box->Foreground() == 0x1B1B1B);

        box->SetChecked(true);
        NOPEUI_CHECK(box->Foreground() == 0x00FF00);

        box->SetChecked(false);
        NOPEUI_CHECK(box->Foreground() == 0x1B1B1B);
    }

    return 0;
}
