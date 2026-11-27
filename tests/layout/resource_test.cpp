#include "../test_check.h"

#include "nopeui/controls/label.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/layout/element.h"
#include "nopeui/theme/theme.h"

int main() {
    // A resource declared on a parent is visible to its descendants only.
    {
        auto panel = nopeui::StackPanel::Create();
        auto label = nopeui::Label::Create(L"Child");
        panel->SetResource(L"AccentBrush", nopeui::PropertyValue(0x0067C0u));
        panel->Add(label);

        nopeui::PropertyValue found;
        NOPEUI_CHECK(panel->FindResourceValue(L"AccentBrush", &found));
        NOPEUI_CHECK(found.Type() == nopeui::PropertyValueType::Color);

        auto sibling = nopeui::Label::Create(L"Outside");
        NOPEUI_CHECK(!sibling->FindResourceValue(L"AccentBrush", &found));
    }

    // Theme palette keys resolve through the theme, so they follow a switch.
    {
        auto label = nopeui::Label::Create(L"Themed");
        label->SetTheme(&nopeui::DarkTheme());

        nopeui::PropertyValue found;
        NOPEUI_CHECK(label->FindResourceValue(L"Accent", &found));
        std::uint32_t color = 0;
        NOPEUI_CHECK(found.AsColor(&color));
        NOPEUI_CHECK(color == 0x60CDFF);

        label->BindThemeResource(L"Foreground", L"Accent");
        NOPEUI_CHECK(label->Foreground() == 0x60CDFF);

        label->SetTheme(&nopeui::LightTheme());
        label->RefreshThemeResources();
        NOPEUI_CHECK(label->Foreground() == 0x0067C0);
    }

    // An element-local resource wins over the theme key of the same name.
    {
        auto panel = nopeui::StackPanel::Create();
        panel->SetResource(L"Accent", nopeui::PropertyValue(0x112233u));
        panel->SetTheme(&nopeui::LightTheme());

        nopeui::PropertyValue found;
        NOPEUI_CHECK(panel->FindResourceValue(L"Accent", &found));
        std::uint32_t color = 0;
        NOPEUI_CHECK(found.AsColor(&color));
        NOPEUI_CHECK(color == 0x112233);
    }

    // Unknown keys are reported, not silently defaulted.
    {
        auto label = nopeui::Label::Create(L"Missing");
        nopeui::PropertyValue found;
        NOPEUI_CHECK(!label->FindResourceValue(L"NothingHere", &found));
    }

    return 0;
}
