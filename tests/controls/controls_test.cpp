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

#include "nopeui/base/property_value.h"
#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/canvas.h"
#include "nopeui/controls/check_box.h"
#include "nopeui/controls/content_presenter.h"
#include "nopeui/controls/context_menu.h"
#include "nopeui/controls/flyout.h"
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
#include "nopeui/graphics/offscreen.h"
#include "nopeui/layout/attached.h"
#include "nopeui/layout/layout_types.h"
#include "nopeui/theme/control_template.h"

namespace {

constexpr float kWide = 480.0f;
constexpr float kTall = 320.0f;

std::string TempPath(const char* name) {
    char directory[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, directory) == 0) {
        return std::string(name);
    }
    return std::string(directory) + name;
}

}  // namespace

int main() {
    auto surface_result = nopeui::graphics::OffscreenSurface::Create(
        static_cast<int>(kWide), static_cast<int>(kTall));
    NOPEUI_CHECK(surface_result.ok());
    auto& surface = surface_result.value();
    ID2D1RenderTarget* target = surface->Target();
    NOPEUI_CHECK(target != nullptr);
    NOPEUI_CHECK(surface->BeginDraw().ok());

    // Determinate, indeterminate and disabled progress bars.
    {
        auto bar = nopeui::ProgressBar::Create(0.25f);
        nopeui::Element& e = *bar;
        NOPEUI_CHECK(bar->Value() == 0.25f);
        bar->SetValue(1.5f);
        NOPEUI_CHECK(bar->Value() == 1.0f);
        bar->SetValue(-1.0f);
        NOPEUI_CHECK(bar->Value() == 0.0f);
        bar->SetValue(-1.0f);  // No change, no invalidate.
        NOPEUI_CHECK(!bar->IsIndeterminate());

        e.Measure(nopeui::Size{120.0f, 20.0f});
        NOPEUI_CHECK(e.DesiredSize().height == 6.0f);
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 120.0f, 6.0f});

        bar->SetValue(0.5f);
        e.Paint(target);
        bar->SetIndeterminate(true);
        NOPEUI_CHECK(bar->IsIndeterminate());
        bar->SetIndeterminate(true);  // No change.
        e.Paint(target);
        bar->SetIndeterminate(false);
        e.SetEnabled(false);
        e.Paint(target);
        e.SetEnabled(true);
        bar->SetValue(0.0f);
        e.Paint(target);

        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.GetProperty(L"Value", &value));
        float number = 0.0f;
        NOPEUI_CHECK(value.AsFloat(&number));
        NOPEUI_CHECK(number == 0.0f);
        NOPEUI_CHECK(e.GetProperty(L"IsIndeterminate", &value));
        NOPEUI_CHECK(e.SetProperty(L"Value", nopeui::PropertyValue(0.75f)));
        NOPEUI_CHECK(!e.SetProperty(L"Value", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(e.SetProperty(L"IsIndeterminate", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(!e.SetProperty(L"Missing", nopeui::PropertyValue(1.0f)));
        NOPEUI_CHECK(e.AccessibilityRangeValue() == 75.0);
        NOPEUI_CHECK(e.AccessibilityRangeMinimum() == 0.0);
        NOPEUI_CHECK(e.AccessibilityRangeMaximum() == 100.0);
        NOPEUI_CHECK(e.IsAccessibilityRangeReadOnly());
        NOPEUI_CHECK(e.AccessibilityRangeSmallChange() == 0.0);
        NOPEUI_CHECK(e.AccessibilityRole() == nopeui::AccessibleRole::ProgressBar);
    }

    // Toggle button reacts to pointer, keys and automation.
    {
        auto button = nopeui::ToggleButton::Create(L"Pin");
        nopeui::Element& e = *button;
        int changes = 0;
        button->SetOnCheckedChanged([&changes](bool) { ++changes; });

        e.Measure(nopeui::Size{120.0f, 32.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 120.0f, 32.0f});

        e.OnPointerEnter();
        e.Paint(target);
        e.OnPointerDown(nopeui::Point{60.0f, 16.0f});
        e.Paint(target);
        e.OnPointerUp(nopeui::Point{60.0f, 16.0f});
        NOPEUI_CHECK(button->IsChecked());
        NOPEUI_CHECK(changes == 1);
        e.Paint(target);

        // A press that ends outside does not toggle.
        e.OnPointerDown(nopeui::Point{60.0f, 16.0f});
        e.OnPointerUp(nopeui::Point{300.0f, 300.0f});
        NOPEUI_CHECK(button->IsChecked());

        e.OnKeyDown(VK_SPACE);
        e.OnKeyUp(VK_SPACE);
        NOPEUI_CHECK(!button->IsChecked());
        e.OnKeyDown(VK_RETURN);
        e.OnKeyUp(VK_RETURN);
        NOPEUI_CHECK(button->IsChecked());
        NOPEUI_CHECK(!e.OnKeyDown(VK_LEFT));
        NOPEUI_CHECK(!e.OnKeyUp('A'));

        e.SetFocused(true);
        e.Paint(target);
        e.SetFocused(false);

        NOPEUI_CHECK(e.HasAccessibilityInvoke());
        NOPEUI_CHECK(e.AccessibilityToggleOn());
        e.ToggleAccessibility();
        NOPEUI_CHECK(!button->IsChecked());
        e.InvokeAccessibility();
        NOPEUI_CHECK(e.AccessibilityName() == L"Pin");

        e.SetEnabled(false);
        e.Paint(target);
        NOPEUI_CHECK(!e.OnKeyDown(VK_SPACE));
        NOPEUI_CHECK(!e.OnKeyUp(VK_SPACE));
        e.ToggleAccessibility();
        NOPEUI_CHECK(button->IsChecked());
        e.InvokeAccessibility();
        e.SetEnabled(true);

        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.SetProperty(L"IsChecked", nopeui::PropertyValue(false)));
        button->SetChecked(true);
        NOPEUI_CHECK(button->IsChecked());
        NOPEUI_CHECK(e.GetProperty(L"IsChecked", &value));
        NOPEUI_CHECK(e.GetProperty(L"Text", &value));
        std::wstring text;
        NOPEUI_CHECK(value.AsString(&text) && text == L"Pin");
        NOPEUI_CHECK(!e.SetProperty(L"IsChecked", nopeui::PropertyValue(1.0f)));
        e.OnPointerLeave();
    }

    // Toggle switch shares the check state contract but toggles on Space only.
    {
        auto toggle = nopeui::ToggleSwitch::Create(L"Wi-Fi");
        nopeui::Element& e = *toggle;
        int changes = 0;
        toggle->SetOnCheckedChanged([&changes](bool) { ++changes; });
        e.Measure(nopeui::Size{180.0f, 32.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 180.0f, 32.0f});

        e.Paint(target);
        e.OnPointerEnter();
        e.OnPointerDown(nopeui::Point{30.0f, 16.0f});
        e.Paint(target);
        e.OnPointerUp(nopeui::Point{30.0f, 16.0f});
        NOPEUI_CHECK(toggle->IsChecked());
        NOPEUI_CHECK(changes == 1);
        e.OnPointerUp(nopeui::Point{30.0f, 16.0f});

        e.OnKeyDown(VK_SPACE);
        e.OnKeyUp(VK_SPACE);
        NOPEUI_CHECK(!toggle->IsChecked());
        NOPEUI_CHECK(!e.OnKeyDown(VK_RETURN));
        NOPEUI_CHECK(!e.AccessibilityToggleOn());
        e.ToggleAccessibility();
        NOPEUI_CHECK(toggle->IsChecked());

        toggle->SetChecked(false);
        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.SetProperty(L"IsChecked", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(e.GetProperty(L"IsChecked", &value));
        NOPEUI_CHECK(e.GetProperty(L"Label", &value));
        NOPEUI_CHECK(!e.GetProperty(L"Missing", &value));
        NOPEUI_CHECK(!e.SetProperty(L"Label", nopeui::PropertyValue(1.0f)));
        e.SetEnabled(false);
        e.ToggleAccessibility();
        NOPEUI_CHECK(toggle->IsChecked());
        e.SetEnabled(true);
        e.Paint(target);
    }

    // Radio buttons deselect their siblings under the same parent.
    {
        auto panel = nopeui::StackPanel::Create(4.0f);
        auto first = nopeui::RadioButton::Create(L"One");
        auto second = nopeui::RadioButton::Create(L"Two");
        nopeui::Element& first_e = *first;
        nopeui::Element& second_e = *second;
        int first_changes = 0;
        int second_changes = 0;
        first->SetOnCheckedChanged([&first_changes](bool) { ++first_changes; });
        second->SetOnCheckedChanged([&second_changes](bool) { ++second_changes; });
        panel->Add(first);
        panel->Add(second);
        panel->Measure(nopeui::Size{200.0f, 120.0f});
        panel->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 120.0f});

        first_e.OnPointerEnter();
        first_e.OnPointerDown(nopeui::Point{20.0f, 10.0f});
        first_e.OnPointerUp(nopeui::Point{20.0f, 10.0f});
        NOPEUI_CHECK(first->IsChecked());
        NOPEUI_CHECK(!second->IsChecked());
        first_e.Paint(target);

        const nopeui::Point second_point{
            second->Bounds().x + 20.0f, second->Bounds().y + 10.0f};
        second_e.OnPointerDown(second_point);
        second_e.OnPointerUp(second_point);
        NOPEUI_CHECK(second->IsChecked());
        NOPEUI_CHECK(!first->IsChecked());
        NOPEUI_CHECK(first_changes >= 2);
        NOPEUI_CHECK(second_changes >= 1);
        second_e.Paint(target);

        // Keyboard and automation also select.
        first_e.SetFocused(true);
        first_e.OnKeyDown(VK_SPACE);
        first_e.OnKeyUp(VK_SPACE);
        NOPEUI_CHECK(first->IsChecked());
        NOPEUI_CHECK(!second->IsChecked());
        first_e.OnKeyUp(VK_SPACE);
        first->SetChecked(false);
        second_e.ToggleAccessibility();
        NOPEUI_CHECK(second->IsChecked());
        NOPEUI_CHECK(second_e.AccessibilityToggleOn());
        NOPEUI_CHECK(second_e.AccessibilityName() == L"Two");

        nopeui::PropertyValue value;
        NOPEUI_CHECK(first_e.SetProperty(L"IsChecked", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(!second->IsChecked());
        NOPEUI_CHECK(first_e.GetProperty(L"Label", &value));
        NOPEUI_CHECK(!first_e.SetProperty(L"Label", nopeui::PropertyValue(2.0f)));
        panel->Paint(target);
        first_e.OnPointerLeave();
    }

    // Slider drag, keys, range and automation value.
    {
        auto slider = nopeui::Slider::Create(0.5, 0.0, 1.0);
        nopeui::Element& e = *slider;
        double last_value = -1.0;
        slider->SetOnValueChanged([&last_value](double value) { last_value = value; });
        e.Measure(nopeui::Size{200.0f, 24.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 24.0f});

        e.Paint(target);
        e.OnPointerEnter();
        e.OnPointerDown(nopeui::Point{100.0f, 12.0f});
        NOPEUI_CHECK(slider->Value() > 0.4 && slider->Value() < 0.6);
        e.OnPointerMove(nopeui::Point{180.0f, 12.0f});
        NOPEUI_CHECK(slider->Value() > 0.8);
        e.Paint(target);
        e.OnPointerUp(nopeui::Point{180.0f, 12.0f});
        e.OnPointerMove(nopeui::Point{10.0f, 12.0f});  // Not dragging.
        NOPEUI_CHECK(last_value > 0.8);

        const double before_step = slider->Value();
        e.OnKeyDown(VK_RIGHT);
        NOPEUI_CHECK(slider->Value() > before_step);
        NOPEUI_CHECK(slider->Value() < 1.0);
        e.OnKeyDown(VK_PRIOR);
        NOPEUI_CHECK(slider->Value() == 1.0);
        e.OnKeyDown(VK_LEFT);
        e.OnKeyDown(VK_LEFT);
        NOPEUI_CHECK(slider->Value() < 1.0);
        e.OnKeyDown(VK_HOME);
        NOPEUI_CHECK(slider->Value() == 0.0);
        e.OnKeyDown(VK_PRIOR);
        e.OnKeyDown(VK_NEXT);
        e.OnKeyDown(VK_END);
        NOPEUI_CHECK(slider->Value() == 1.0);
        NOPEUI_CHECK(!e.OnKeyDown('A'));

        slider->SetRange(0.0, 10.0);
        slider->SetValue(50.0);
        NOPEUI_CHECK(slider->Value() == 10.0);
        slider->SetValue(-5.0);
        NOPEUI_CHECK(slider->Value() == 0.0);
        NOPEUI_CHECK(e.SetAccessibilityRangeValue(5.0));
        NOPEUI_CHECK(e.AccessibilityRangeValue() == 5.0);
        NOPEUI_CHECK(e.AccessibilityRangeMinimum() == 0.0);
        NOPEUI_CHECK(e.AccessibilityRangeMaximum() == 10.0);
        NOPEUI_CHECK(e.AccessibilityRangeSmallChange() == 0.5);
        NOPEUI_CHECK(e.AccessibilityRangeLargeChange() == 2.5);
        e.SetFocused(true);
        e.Paint(target);
        e.SetEnabled(false);
        NOPEUI_CHECK(!e.SetAccessibilityRangeValue(1.0));
        NOPEUI_CHECK(!e.OnKeyDown(VK_RIGHT));
        e.Paint(target);
        e.SetEnabled(true);

        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.SetProperty(L"Value", nopeui::PropertyValue(2.0)));
        NOPEUI_CHECK(e.GetProperty(L"Value", &value));
        double number = 0.0;
        NOPEUI_CHECK(value.AsDouble(&number));
        NOPEUI_CHECK(number == 2.0);
        NOPEUI_CHECK(!e.SetProperty(L"Value", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(e.SetProperty(L"Maximum", nopeui::PropertyValue(20.0)));
        NOPEUI_CHECK(e.SetProperty(L"Minimum", nopeui::PropertyValue(1.0)));
        NOPEUI_CHECK(!e.GetProperty(L"Missing", &value));
        e.OnPointerLeave();
    }

    // Check box shares input paths and reads template parts.
    {
        auto box = nopeui::CheckBox::Create(L"Remember");
        nopeui::Element& e = *box;
        int changes = 0;
        box->SetOnCheckedChanged([&changes](bool) { ++changes; });
        e.Measure(nopeui::Size{160.0f, 24.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 160.0f, 24.0f});

        e.Paint(target);
        e.OnPointerDown(nopeui::Point{20.0f, 12.0f});
        e.Paint(target);
        e.OnPointerUp(nopeui::Point{20.0f, 12.0f});
        NOPEUI_CHECK(box->IsChecked());
        NOPEUI_CHECK(changes == 1);
        e.Paint(target);

        e.OnKeyDown(VK_SPACE);
        e.OnKeyUp(VK_SPACE);
        NOPEUI_CHECK(!box->IsChecked());
        NOPEUI_CHECK(!e.OnKeyDown(VK_RETURN));
        e.OnPointerDown(nopeui::Point{20.0f, 12.0f});
        e.OnPointerUp(nopeui::Point{300.0f, 300.0f});
        NOPEUI_CHECK(!box->IsChecked());

        e.ToggleAccessibility();
        NOPEUI_CHECK(box->IsChecked());
        NOPEUI_CHECK(e.AccessibilityToggleOn());
        NOPEUI_CHECK(e.AccessibilityName() == L"Remember");
        e.SetEnabled(false);
        e.ToggleAccessibility();
        NOPEUI_CHECK(box->IsChecked());
        e.SetEnabled(true);

        auto control_template = nopeui::ControlTemplate::Create();
        nopeui::TemplatePart part;
        part.background = 0x112233;
        part.border = 0x445566;
        part.foreground = 0x778899;
        control_template->SetPart(L"Box", part);
        control_template->SetPart(L"Check", part);
        control_template->SetPart(L"Content", part);
        e.ApplyControlTemplate(*control_template);
        e.Paint(target);

        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.SetProperty(L"IsChecked", nopeui::PropertyValue(false)));
        NOPEUI_CHECK(e.GetProperty(L"Label", &value));
        NOPEUI_CHECK(!e.SetProperty(L"Label", nopeui::PropertyValue(1.0f)));
        NOPEUI_CHECK(!e.GetProperty(L"Missing", &value));
        e.OnPointerEnter();
        e.SetFocused(true);
        e.Paint(target);
        e.OnPointerLeave();
    }

    // Button click, keys, automation and the templated visual tree.
    {
        auto button = nopeui::Button::Create(L"Apply");
        nopeui::Element& e = *button;
        int clicks = 0;
        button->SetOnClick([&clicks] { ++clicks; });
        e.Measure(nopeui::Size{120.0f, 32.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 120.0f, 32.0f});

        e.Paint(target);
        e.OnPointerEnter();
        e.Paint(target);
        e.OnPointerDown(nopeui::Point{60.0f, 16.0f});
        e.Paint(target);
        e.OnPointerUp(nopeui::Point{60.0f, 16.0f});
        NOPEUI_CHECK(clicks == 1);
        e.OnPointerUp(nopeui::Point{60.0f, 16.0f});

        e.OnKeyDown(VK_SPACE);
        e.OnKeyUp(VK_SPACE);
        NOPEUI_CHECK(clicks == 2);
        NOPEUI_CHECK(!e.OnKeyDown(VK_LEFT));
        e.InvokeAccessibility();
        NOPEUI_CHECK(clicks == 3);
        e.SetEnabled(false);
        e.InvokeAccessibility();
        NOPEUI_CHECK(clicks == 3);
        NOPEUI_CHECK(!e.OnKeyDown(VK_SPACE));
        e.Paint(target);
        e.SetEnabled(true);
        e.SetFocused(true);
        e.Paint(target);
        e.SetFocused(false);
        e.OnPointerLeave();

        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.GetProperty(L"Text", &value));
        std::wstring text;
        NOPEUI_CHECK(value.AsString(&text) && text == L"Apply");
        NOPEUI_CHECK(e.SetProperty(L"CornerRadius", nopeui::PropertyValue(8.0f)));
        NOPEUI_CHECK(!e.SetProperty(L"CornerRadius", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(!e.SetProperty(L"Text", nopeui::PropertyValue(3.0f)));
        NOPEUI_CHECK(!e.GetProperty(L"Missing", &value));

        auto presenter = nopeui::ContentPresenter::Create();
        auto root = nopeui::Border::Create();
        root->SetChild(presenter);
        auto control_template = nopeui::ControlTemplate::Create();
        control_template->SetFactory([root, presenter] {
            nopeui::TemplateInstance instance;
            instance.root = root;
            nopeui::TemplateBinding binding;
            binding.target = presenter;
            binding.target_property = L"Text";
            binding.source_property = L"Text";
            instance.bindings.push_back(binding);
            nopeui::TemplateBinding radius;
            radius.target = root;
            radius.target_property = L"CornerRadius";
            radius.source_property = L"CornerRadius";
            instance.bindings.push_back(radius);
            return instance;
        });
        e.ApplyControlTemplate(*control_template);
        NOPEUI_CHECK(button->TemplateRoot() != nullptr);
        e.Measure(nopeui::Size{160.0f, 40.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 160.0f, 40.0f});
        e.Paint(target);
        NOPEUI_CHECK(presenter->Text() == L"Apply");
        // A property change flows through the template binding.
        NOPEUI_CHECK(e.SetProperty(L"CornerRadius", nopeui::PropertyValue(3.0f)));
        nopeui::PropertyValue corner;
        NOPEUI_CHECK(root->GetProperty(L"CornerRadius", &corner));
        float corner_value = 0.0f;
        NOPEUI_CHECK(corner.AsFloat(&corner_value) && corner_value == 3.0f);
    }

    // Labels cover wrap, ellipsis, color and template-free painting.
    {
        auto plain = nopeui::Label::Create(L"Plain label");
        nopeui::Element& e = *plain;
        e.Measure(nopeui::Size{200.0f, 60.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 60.0f});
        e.Paint(target);

        nopeui::graphics::TextStyle wrap;
        wrap.wrap = true;
        wrap.size = 16.0f;
        auto wrapped = nopeui::Label::Create(L"A longer label that wraps onto several lines", wrap);
        nopeui::Element& wrapped_e = *wrapped;
        wrapped_e.Measure(nopeui::Size{100.0f, 200.0f});
        NOPEUI_CHECK(wrapped_e.DesiredSize().height > 20.0f);
        wrapped_e.Arrange(nopeui::Rect{0.0f, 0.0f, 100.0f, 80.0f});
        wrapped_e.Paint(target);

        nopeui::graphics::TextStyle ellipsis;
        ellipsis.ellipsis = true;
        auto trimmed = nopeui::Label::Create(L"This text is much wider than the slot", ellipsis);
        nopeui::Element& trimmed_e = *trimmed;
        trimmed_e.Measure(nopeui::Size{60.0f, 40.0f});
        NOPEUI_CHECK(trimmed_e.DesiredSize().width <= 60.0f);
        trimmed_e.Arrange(nopeui::Rect{0.0f, 0.0f, 60.0f, 20.0f});
        trimmed_e.Paint(target);

        trimmed->SetColor(0xCC0000);
        trimmed_e.Paint(target);
        wrapped->SetTextStyle(nopeui::graphics::TextStyle{});
        wrapped_e.Paint(target);

        nopeui::PropertyValue value;
        NOPEUI_CHECK(trimmed_e.SetProperty(L"Color", nopeui::PropertyValue(0x00FF00u)));
        NOPEUI_CHECK(!trimmed_e.SetProperty(L"Color", nopeui::PropertyValue(1.0f)));
        NOPEUI_CHECK(!trimmed_e.GetProperty(L"Missing", &value));
        NOPEUI_CHECK(trimmed_e.GetProperty(L"Text", &value));
        std::wstring label_text;
        NOPEUI_CHECK(value.AsString(&label_text));
        NOPEUI_CHECK(trimmed_e.AccessibilityRole() == nopeui::AccessibleRole::Text);
        NOPEUI_CHECK(trimmed_e.AccessibilityName() == label_text);
    }

    // Border paints its variants and lays out a child.
    {
        auto border = nopeui::Border::Create();
        nopeui::Element& e = *border;
        auto child = nopeui::Label::Create(L"Inside");
        border->SetChild(child);
        border->SetPadding(nopeui::Thickness{8.0f, 6.0f, 8.0f, 6.0f});
        border->SetCornerRadius(6.0f);
        border->SetBackground(0xFFF0E0);
        border->SetBorderColor(0xA0A0A0);
        e.Measure(nopeui::Size{200.0f, 80.0f});
        NOPEUI_CHECK(e.DesiredSize().width > 16.0f);
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 80.0f});
        e.Paint(target);
        NOPEUI_CHECK(child->Bounds().x > 8.0f && child->Bounds().x < 10.0f);
        NOPEUI_CHECK(child->Bounds().y > 6.0f && child->Bounds().y < 8.0f);

        border->UseSurfaceBackground(true);
        e.Paint(target);
        border->SetBorderVisible(false);
        e.Paint(target);
        border->SetBorderVisible(true);
        border->SetBackground(0x00000000);
        e.Paint(target);

        // Removing the child leaves the border alone.
        border->SetChild(nopeui::Ref<nopeui::Element>{});
        e.Measure(nopeui::Size{100.0f, 40.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 100.0f, 40.0f});
        e.Paint(target);

        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.SetProperty(L"CornerRadius", nopeui::PropertyValue(2.0f)));
        NOPEUI_CHECK(e.SetProperty(L"Background", nopeui::PropertyValue(0x123456u)));
        NOPEUI_CHECK(e.SetProperty(L"BorderColor", nopeui::PropertyValue(0x654321u)));
        NOPEUI_CHECK(e.SetProperty(L"BorderVisible", nopeui::PropertyValue(false)));
        NOPEUI_CHECK(e.SetProperty(L"SurfaceBackground", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(e.GetProperty(L"Background", &value));
        NOPEUI_CHECK(!e.SetProperty(L"CornerRadius", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(!e.GetProperty(L"Missing", &value));
        e.Paint(target);

        auto hit = nopeui::Border::Create();
        nopeui::Element& hit_e = *hit;
        hit_e.Measure(nopeui::Size{80.0f, 40.0f});
        hit_e.Arrange(nopeui::Rect{0.0f, 0.0f, 80.0f, 40.0f});
        NOPEUI_CHECK(hit_e.HitTest(nopeui::Point{10.0f, 10.0f}) == hit.Get());
        NOPEUI_CHECK(hit_e.HitTest(nopeui::Point{200.0f, 200.0f}) == nullptr);
    }

    // Flyout measures with and without content and paints both.
    {
        auto flyout = nopeui::Flyout::Create();
        nopeui::Element& e = *flyout;
        e.Measure(nopeui::Size{300.0f, 200.0f});
        NOPEUI_CHECK(e.DesiredSize().width == 160.0f);
        NOPEUI_CHECK(e.DesiredSize().height == 64.0f);
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 160.0f, 64.0f});
        e.Paint(target);

        auto content = nopeui::Label::Create(L"Flyout content");
        flyout->SetContent(content);
        e.Measure(nopeui::Size{300.0f, 200.0f});
        NOPEUI_CHECK(e.DesiredSize().width > 32.0f);
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 80.0f});
        e.Paint(target);
        NOPEUI_CHECK(content->Bounds().x == 16.0f);
        NOPEUI_CHECK(content->Bounds().y == 14.0f);

        flyout->SetContent(nopeui::Ref<nopeui::Element>{});
        e.Measure(nopeui::Size{300.0f, 200.0f});
        e.Paint(target);
    }

    // Context menu hit testing, keyboard, wheel and automation.
    {
        auto menu = nopeui::ContextMenu::Create();
        nopeui::Element& e = *menu;
        int activated = -1;
        for (int index = 0; index < 6; ++index) {
            menu->AddItem(L"Item " + std::to_wstring(index),
                          [&activated, index] { activated = index; });
        }
        e.Measure(nopeui::Size{220.0f, 120.0f});
        NOPEUI_CHECK(e.DesiredSize().height == 120.0f);
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 220.0f, 120.0f});
        e.OnPopupOpened();
        e.Paint(target);

        e.OnPointerEnter();
        e.OnPointerMove(nopeui::Point{40.0f, 51.0f});
        e.Paint(target);
        e.OnPointerDown(nopeui::Point{40.0f, 51.0f});
        e.OnPointerUp(nopeui::Point{40.0f, 51.0f});
        NOPEUI_CHECK(activated == 1);

        e.OnPointerMove(nopeui::Point{400.0f, 400.0f});
        e.OnPointerDown(nopeui::Point{400.0f, 400.0f});
        e.OnPointerUp(nopeui::Point{400.0f, 400.0f});
        NOPEUI_CHECK(activated == 1);

        // Wheel scrolls, and End then Return activates the last row.
        e.OnWheel(-WHEEL_DELTA);
        e.OnWheel(WHEEL_DELTA);
        NOPEUI_CHECK(e.OnKeyDown(VK_END));
        NOPEUI_CHECK(e.OnKeyDown(VK_RETURN));
        NOPEUI_CHECK(activated == 5);
        NOPEUI_CHECK(e.OnKeyDown(VK_UP));
        NOPEUI_CHECK(e.OnKeyDown(VK_HOME));
        NOPEUI_CHECK(e.OnKeyDown(VK_DOWN));
        NOPEUI_CHECK(e.OnKeyDown(VK_SPACE));
        NOPEUI_CHECK(activated == 1);
        NOPEUI_CHECK(!e.OnKeyDown('A'));
        e.Paint(target);

        std::vector<std::wstring> items;
        e.CollectAccessibleMenuItems(items);
        NOPEUI_CHECK(items.size() == 6);
        NOPEUI_CHECK(!e.AccessibilityMenuItemBounds(0).IsEmpty());
        NOPEUI_CHECK(e.AccessibilityMenuItemBounds(99).IsEmpty());
        NOPEUI_CHECK(e.InvokeAccessibilityMenuItem(2));
        NOPEUI_CHECK(activated == 2);
        NOPEUI_CHECK(!e.InvokeAccessibilityMenuItem(99));
        e.OnPointerLeave();
    }

    // Menu bar hover, activation, keys and automation.
    {
        auto bar = nopeui::MenuBar::Create();
        nopeui::Element& e = *bar;
        bar->AddMenu(L"File", {nopeui::MenuCommand{L"Open", [] {}}, nopeui::MenuCommand{L"Exit", [] {}}});
        bar->AddMenu(L"Edit", {nopeui::MenuCommand{L"Undo", [] {}}});
        NOPEUI_CHECK(bar->MenuCount() == 2);
        e.Measure(nopeui::Size{400.0f, 30.0f});
        NOPEUI_CHECK(e.DesiredSize().height == 30.0f);
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 400.0f, 30.0f});
        e.Paint(target);

        e.OnPointerMove(nopeui::Point{20.0f, 15.0f});
        e.Paint(target);
        e.OnPointerDown(nopeui::Point{20.0f, 15.0f});
        e.OnPointerUp(nopeui::Point{20.0f, 15.0f});
        NOPEUI_CHECK(bar->ActiveMenuIndex() == 0);
        e.Paint(target);

        e.OnPointerDown(nopeui::Point{20.0f, 15.0f});
        e.OnPointerUp(nopeui::Point{20.0f, 15.0f});
        NOPEUI_CHECK(bar->ActiveMenuIndex() == -1);
        e.OnPointerDown(nopeui::Point{20.0f, 15.0f});
        e.OnPointerUp(nopeui::Point{400.0f, 15.0f});
        NOPEUI_CHECK(bar->ActiveMenuIndex() == -1);

        NOPEUI_CHECK(e.OnKeyDown(VK_RIGHT));
        NOPEUI_CHECK(bar->ActiveMenuIndex() == 0);
        NOPEUI_CHECK(e.OnKeyDown(VK_RIGHT));
        NOPEUI_CHECK(bar->ActiveMenuIndex() == 1);
        NOPEUI_CHECK(e.OnKeyDown(VK_ESCAPE));
        NOPEUI_CHECK(bar->ActiveMenuIndex() == -1);
        NOPEUI_CHECK(e.OnKeyDown(VK_DOWN));
        NOPEUI_CHECK(bar->ActiveMenuIndex() == 0);
        NOPEUI_CHECK(e.OnKeyDown(VK_ESCAPE));
        NOPEUI_CHECK(e.OnKeyDown(VK_LEFT));
        NOPEUI_CHECK(bar->ActiveMenuIndex() == 1);
        NOPEUI_CHECK(!e.OnKeyDown('A'));
        e.OnPointerLeave();
        e.SetFocused(true);
        e.Paint(target);
        e.SetFocused(false);

        std::vector<std::wstring> items;
        e.CollectAccessibleMenuItems(items);
        NOPEUI_CHECK(items.size() == 2);
        NOPEUI_CHECK(!e.AccessibilityMenuItemBounds(0).IsEmpty());
        NOPEUI_CHECK(e.AccessibilityMenuItemBounds(5).IsEmpty());
        NOPEUI_CHECK(!e.AccessibilityMenuItemExpanded(0));
        NOPEUI_CHECK(e.SetAccessibilityMenuItemExpanded(0, true));
        NOPEUI_CHECK(e.AccessibilityMenuItemExpanded(0));
        NOPEUI_CHECK(e.SetAccessibilityMenuItemExpanded(0, false));
        NOPEUI_CHECK(!e.AccessibilityMenuItemExpanded(0));
        NOPEUI_CHECK(!e.SetAccessibilityMenuItemExpanded(9, true));
        NOPEUI_CHECK(e.InvokeAccessibilityMenuItem(1));
        NOPEUI_CHECK(bar->ActiveMenuIndex() == 1);

        e.SetEnabled(false);
        NOPEUI_CHECK(!e.OnKeyDown(VK_RIGHT));
        e.Paint(target);
        e.SetEnabled(true);
    }

    // Scroll viewer: vertical and horizontal bars, wheel, keys and drag.
    {
        auto viewer = nopeui::ScrollViewer::Create();
        nopeui::Element& e = *viewer;
        auto content = nopeui::StackPanel::Create(0.0f);
        for (int index = 0; index < 10; ++index) {
            auto row = nopeui::Border::Create();
            row->SetWidth(100.0f);
            row->SetHeight(40.0f);
            content->Add(row);
        }
        viewer->SetChild(content);
        e.Measure(nopeui::Size{100.0f, 80.0f});
        NOPEUI_CHECK(e.DesiredSize().height == 80.0f);
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 100.0f, 80.0f});
        NOPEUI_CHECK(viewer->VerticalOffset() == 0.0f);
        e.Paint(target);

        e.OnWheel(-WHEEL_DELTA);
        NOPEUI_CHECK(viewer->VerticalOffset() > 0.0f);
        e.Paint(target);
        e.OnKeyDown(VK_END);
        NOPEUI_CHECK(viewer->VerticalOffset() == 320.0f);
        e.OnKeyDown(VK_HOME);
        NOPEUI_CHECK(viewer->VerticalOffset() == 0.0f);
        e.OnKeyDown(VK_NEXT);
        e.OnKeyDown(VK_PRIOR);
        viewer->SetVerticalOffset(1000.0f);
        NOPEUI_CHECK(viewer->VerticalOffset() == 320.0f);
        viewer->SetVerticalOffset(-5.0f);
        NOPEUI_CHECK(viewer->VerticalOffset() == 0.0f);

        // Drag the thumb downward.
        e.OnPointerEnter();
        e.Paint(target);
        e.OnPointerDown(nopeui::Point{92.0f, 10.0f});
        e.OnPointerMove(nopeui::Point{92.0f, 40.0f});
        NOPEUI_CHECK(viewer->VerticalOffset() > 0.0f);
        e.Paint(target);
        e.OnPointerUp(nopeui::Point{92.0f, 40.0f});
        e.OnPointerUp(nopeui::Point{92.0f, 40.0f});
        e.OnPointerLeave();

        // BringChildIntoView scrolls enough for the last row.
        viewer->SetVerticalOffset(0.0f);
        e.BringChildIntoView(content->Children().back().Get());
        NOPEUI_CHECK(viewer->VerticalOffset() > 0.0f);

        NOPEUI_CHECK(e.AccessibilityVerticallyScrollable());
        NOPEUI_CHECK(!e.AccessibilityHorizontallyScrollable());
        NOPEUI_CHECK(e.AccessibilityVerticalScrollPercent() > 0.0);
        NOPEUI_CHECK(e.AccessibilityHorizontalScrollPercent() == -1.0);
        NOPEUI_CHECK(e.AccessibilityVerticalViewSize() > 0.0);
        NOPEUI_CHECK(e.AccessibilityHorizontalViewSize() == 100.0);
        NOPEUI_CHECK(e.ScrollAccessibilityVertical(-10.0));
        NOPEUI_CHECK(!e.ScrollAccessibilityHorizontal(10.0));
        NOPEUI_CHECK(e.SetAccessibilityScrollPercent(-1.0, 0.0));

        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.GetProperty(L"VerticalOffset", &value));
        NOPEUI_CHECK(e.SetProperty(L"VerticalOffset", nopeui::PropertyValue(10.0f)));
        NOPEUI_CHECK(!e.SetProperty(L"VerticalOffset", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(e.SetProperty(L"HorizontalOffset", nopeui::PropertyValue(0.0f)));
        NOPEUI_CHECK(!e.SetProperty(L"Missing", nopeui::PropertyValue(1.0f)));
        e.SetEnabled(false);
        NOPEUI_CHECK(!e.OnWheel(-WHEEL_DELTA));
        NOPEUI_CHECK(!e.OnKeyDown(VK_END));
        e.SetEnabled(true);
    }

    {
        // Wide content turns on the horizontal bar and its drag path.
        auto viewer = nopeui::ScrollViewer::Create();
        nopeui::Element& e = *viewer;
        auto content = nopeui::StackPanel::Create(0.0f);
        content->SetOrientation(nopeui::Orientation::Horizontal);
        for (int index = 0; index < 8; ++index) {
            auto block = nopeui::Border::Create();
            block->SetWidth(60.0f);
            block->SetHeight(20.0f);
            content->Add(block);
        }
        viewer->SetChild(content);
        e.Measure(nopeui::Size{100.0f, 60.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 100.0f, 60.0f});
        e.Paint(target);

        e.OnWheel(-WHEEL_DELTA);
        NOPEUI_CHECK(viewer->HorizontalOffset() > 0.0f);
        NOPEUI_CHECK(e.OnKeyDown(VK_END));
        NOPEUI_CHECK(viewer->HorizontalOffset() == 380.0f);
        e.OnKeyDown(VK_HOME);
        NOPEUI_CHECK(viewer->HorizontalOffset() == 0.0f);
        e.OnKeyDown(VK_PRIOR);
        e.OnKeyDown(VK_NEXT);
        e.OnKeyDown(VK_PRIOR);
        NOPEUI_CHECK(!e.OnKeyDown('A'));
        e.Paint(target);

        e.OnPointerDown(nopeui::Point{20.0f, 54.0f});
        e.OnPointerMove(nopeui::Point{60.0f, 54.0f});
        NOPEUI_CHECK(viewer->HorizontalOffset() > 0.0f);
        e.OnPointerUp(nopeui::Point{60.0f, 54.0f});
        e.Paint(target);

        NOPEUI_CHECK(e.AccessibilityHorizontallyScrollable());
        NOPEUI_CHECK(e.AccessibilityVerticalScrollPercent() == -1.0);
        NOPEUI_CHECK(e.ScrollAccessibilityHorizontal(10.0));
        NOPEUI_CHECK(!e.ScrollAccessibilityVertical(10.0));
        NOPEUI_CHECK(e.SetAccessibilityScrollPercent(50.0, -1.0));
        NOPEUI_CHECK(e.AccessibilityHorizontalViewSize() > 0.0);
    }

    // Text box typing, editing, IME and automation.
    {
        auto box = nopeui::TextBox::Create(L"ab");
        nopeui::Element& e = *box;
        int submits = 0;
        box->SetOnSubmit([&submits] { ++submits; });
        box->SetPlaceholder(L"Type here");
        e.Measure(nopeui::Size{200.0f, 32.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 32.0f});
        e.Paint(target);

        NOPEUI_CHECK(!e.OnChar(0x01));
        NOPEUI_CHECK(e.OnChar(L'c'));
        NOPEUI_CHECK(box->Text() == L"abc");
        e.OnKeyDown(VK_BACK);
        NOPEUI_CHECK(box->Text() == L"ab");
        e.OnKeyDown(VK_HOME);
        e.OnKeyDown(VK_DELETE);
        NOPEUI_CHECK(box->Text() == L"b");
        e.OnKeyDown(VK_RIGHT);
        e.OnKeyDown(VK_LEFT);
        e.OnKeyDown(VK_END);
        e.SetAccessibilitySelection(0, 1);
        NOPEUI_CHECK(e.AccessibilitySelectionStart() == 0);
        NOPEUI_CHECK(e.AccessibilitySelectionEnd() == 1);
        e.Paint(target);
        e.OnChar(L'x');
        NOPEUI_CHECK(box->Text() == L"x");
        e.OnKeyDown(VK_RETURN);
        NOPEUI_CHECK(submits == 1);
        NOPEUI_CHECK(!e.OnKeyDown('Q'));

        e.SetFocused(true);
        e.Paint(target);
        e.OnPointerDown(nopeui::Point{30.0f, 16.0f});
        e.OnPointerMove(nopeui::Point{80.0f, 16.0f});
        e.Paint(target);
        e.OnPointerUp(nopeui::Point{80.0f, 16.0f});
        e.OnPointerEnter();
        e.Paint(target);
        e.OnPointerLeave();

        e.OnImeStart();
        e.OnImeComposition(L"ka");
        e.Paint(target);
        e.OnImeComposition(L"");
        e.OnImeComposition(L"ka");
        e.OnImeResult(L"ka");
        NOPEUI_CHECK(box->Text().find(L"ka") != std::wstring::npos);
        e.OnImeResult(L"");
        e.OnImeStart();
        e.OnImeEnd();
        e.Paint(target);
        NOPEUI_CHECK(e.ImeCaretPoint().y > 0.0f);

        NOPEUI_CHECK(e.HasAccessibilityValue());
        NOPEUI_CHECK(e.HasAccessibilityText());
        NOPEUI_CHECK(e.AccessibilityValue() == box->Text());
        NOPEUI_CHECK(e.AccessibilityText() == box->Text());
        NOPEUI_CHECK(e.AccessibilityName() == L"Type here");
        NOPEUI_CHECK(e.SetAccessibilityValue(L"value"));
        NOPEUI_CHECK(box->Text() == L"value");

        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.SetProperty(L"Placeholder", nopeui::PropertyValue(std::wstring())));
        NOPEUI_CHECK(e.AccessibilityName() == L"Text");
        NOPEUI_CHECK(e.GetProperty(L"Text", &value));
        NOPEUI_CHECK(!e.SetProperty(L"Text", nopeui::PropertyValue(1.0f)));
        NOPEUI_CHECK(!e.GetProperty(L"Missing", &value));
        e.SetEnabled(false);
        NOPEUI_CHECK(!e.SetAccessibilityValue(L"blocked"));
        NOPEUI_CHECK(!e.OnChar(L'z'));
        e.Paint(target);
        e.SetEnabled(true);

        auto empty = nopeui::TextBox::Create();
        nopeui::Element& empty_e = *empty;
        empty->SetPlaceholder(L"Placeholder only");
        empty_e.Measure(nopeui::Size{200.0f, 32.0f});
        empty_e.Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 32.0f});
        empty_e.Paint(target);
        empty_e.OnPointerDown(nopeui::Point{10.0f, 16.0f});
        empty_e.OnPointerUp(nopeui::Point{10.0f, 16.0f});
    }

    // Tooltip and the owner-keyed service.
    {
        auto tip = nopeui::ToolTip::Create(L"Helper text");
        nopeui::Element& e = *tip;
        e.Measure(nopeui::Size{200.0f, 60.0f});
        e.Arrange(nopeui::Rect{0.0f, 0.0f, e.DesiredSize().width, e.DesiredSize().height});
        e.Paint(target);
        NOPEUI_CHECK(e.AccessibilityName() == L"Helper text");

        auto owner = nopeui::Label::Create(L"Owner");
        nopeui::ToolTipService::SetToolTip(owner.Get(), L"Owned");
        NOPEUI_CHECK(nopeui::ToolTipService::GetToolTip(owner.Get()) == L"Owned");
        nopeui::ToolTipService::ClearToolTip(owner.Get());
        NOPEUI_CHECK(nopeui::ToolTipService::GetToolTip(owner.Get()).empty());
    }

    // Stack panels and canvas paint their children in both orientations.
    {
        auto vertical = nopeui::StackPanel::Create(10.0f);
        auto first = nopeui::Label::Create(L"First");
        auto second = nopeui::Label::Create(L"Second");
        vertical->Add(first);
        vertical->Add(second);
        vertical->Measure(nopeui::Size{120.0f, 100.0f});
        vertical->Arrange(nopeui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
        NOPEUI_CHECK(second->Bounds().y > 0.0f);
        vertical->Paint(target);

        auto horizontal = nopeui::StackPanel::Create(6.0f);
        horizontal->SetOrientation(nopeui::Orientation::Horizontal);
        horizontal->Add(nopeui::Label::Create(L"A"));
        horizontal->Add(nopeui::Label::Create(L"B"));
        horizontal->Measure(nopeui::Size{200.0f, 40.0f});
        horizontal->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 40.0f});
        horizontal->Paint(target);
        NOPEUI_CHECK(horizontal->Children().size() == 2);

        nopeui::PropertyValue value;
        NOPEUI_CHECK(horizontal->SetProperty(L"Spacing", nopeui::PropertyValue(8.0f)));
        NOPEUI_CHECK(horizontal->SetProperty(L"Orientation", nopeui::PropertyValue(0)));
        NOPEUI_CHECK(horizontal->GetOrientation() == nopeui::Orientation::Vertical);
        NOPEUI_CHECK(horizontal->GetProperty(L"Spacing", &value));
        NOPEUI_CHECK(!horizontal->SetProperty(L"Spacing", nopeui::PropertyValue(true)));
        NOPEUI_CHECK(!horizontal->GetProperty(L"Missing", &value));

        auto canvas = nopeui::Canvas::Create();
        auto child = nopeui::Border::Create();
        child->SetWidth(30.0f);
        child->SetHeight(20.0f);
        child->SetAttachedProperty(
            nopeui::CanvasLeftProperty(), nopeui::PropertyValue(40.0f));
        child->SetAttachedProperty(
            nopeui::CanvasTopProperty(), nopeui::PropertyValue(10.0f));
        canvas->Add(child);
        canvas->Measure(nopeui::Size{200.0f, 120.0f});
        canvas->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 120.0f});
        canvas->Paint(target);
        NOPEUI_CHECK(child->Bounds().x == 40.0f);
        NOPEUI_CHECK(canvas->HitTest(nopeui::Point{45.0f, 15.0f}) == child.Get());
        NOPEUI_CHECK(canvas->HitTest(nopeui::Point{10.0f, 10.0f}) != child.Get());
    }

    // Image control covers every stretch mode, placeholders and file loading.
    {
        nopeui::graphics::Image pixels;
        pixels.width = 3;
        pixels.height = 2;
        pixels.pixels = {0xFF0000FFu, 0x8000FF00u, 0x400000FFu,
                         0x00FFFFFFu, 0xFF112233u, 0xFFFFFFFFu};

        auto image = nopeui::Image::Create();
        nopeui::Element& e = *image;
        NOPEUI_CHECK(image->Stretch() == nopeui::ImageStretch::Uniform);
        e.Measure(nopeui::Size{200.0f, 100.0f});
        NOPEUI_CHECK(e.DesiredSize().width == 160.0f);
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 160.0f, 90.0f});
        e.Paint(target);

        image->SetImage(pixels);
        NOPEUI_CHECK(image->Source().width == 3);
        e.Measure(nopeui::Size{200.0f, 100.0f});
        NOPEUI_CHECK(e.DesiredSize().width == 3.0f);
        e.Arrange(nopeui::Rect{0.0f, 0.0f, 120.0f, 80.0f});
        e.Paint(target);

        image->SetStretch(nopeui::ImageStretch::None);
        e.Paint(target);
        image->SetStretch(nopeui::ImageStretch::Fill);
        e.Paint(target);
        image->SetStretch(nopeui::ImageStretch::UniformToFill);
        e.Paint(target);
        image->SetStretch(nopeui::ImageStretch::Uniform);

        image->SetDisplaySize(nopeui::Size{40.0f, 0.0f});
        e.Measure(nopeui::Size{200.0f, 100.0f});
        NOPEUI_CHECK(e.DesiredSize().width == 40.0f);
        image->SetDisplaySize(nopeui::Size{});
        e.Measure(nopeui::Size{0.0f, 0.0f});
        NOPEUI_CHECK(e.DesiredSize().width == 3.0f);

        nopeui::PropertyValue value;
        NOPEUI_CHECK(e.GetProperty(L"NaturalWidth", &value));
        int natural = 0;
        NOPEUI_CHECK(value.AsInt(&natural) && natural == 3);
        NOPEUI_CHECK(e.SetProperty(
            L"Stretch", nopeui::PropertyValue(static_cast<int>(nopeui::ImageStretch::Fill))));
        NOPEUI_CHECK(image->Stretch() == nopeui::ImageStretch::Fill);
        NOPEUI_CHECK(!e.SetProperty(L"Stretch", nopeui::PropertyValue(7)));
        NOPEUI_CHECK(!e.SetProperty(L"NaturalWidth", nopeui::PropertyValue(5)));
        NOPEUI_CHECK(!e.GetProperty(L"Missing", &value));

        // File load succeeds for a real PNG and fails for a missing one.
        const std::string path = TempPath("nopeui-image-control.png");
        std::vector<std::uint32_t> four{0xFF0000FFu, 0xFF00FF00u, 0xFF0000FFu, 0xFFFFFFFFu};
        NOPEUI_CHECK(nopeui::graphics::SavePng(path, 2, 2, four).ok());
        auto fresh = nopeui::Image::Create();
        NOPEUI_CHECK(fresh->LoadFromFile(path));
        NOPEUI_CHECK(fresh->Source().width == 2);
        NOPEUI_CHECK(!fresh->LoadFromFile(TempPath("nopeui-image-missing.png")));
        auto from_file = nopeui::Image::CreateFromFile(path);
        NOPEUI_CHECK(from_file.ok());
        NOPEUI_CHECK(from_file.value()->Source().height == 2);
        NOPEUI_CHECK(!nopeui::Image::CreateFromFile(TempPath("nopeui-image-missing.png")).ok());
        DeleteFileA(path.c_str());
    }

    NOPEUI_CHECK(surface->EndDraw().ok());
    return 0;
}
