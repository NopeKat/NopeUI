#include "../test_check.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "nopeui/base/geometry.h"
#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/label.h"
#include "nopeui/layout/attached.h"
#include "nopeui/layout/style.h"

namespace {

bool Near(float left, float right) {
    return std::fabs(left - right) < 0.5f;
}

}  // namespace

int main() {
    // Margin is added around the measured content and shifts the bounds.
    {
        auto label = nopeui::Label::Create(L"Margins");
        label->Measure(nopeui::Size{400.0f, 200.0f});
        const float bare_width = label->DesiredSize().width;
        NOPEUI_CHECK(!Near(bare_width, 0.0f));

        label->SetMargin(nopeui::Thickness{10.0f, 20.0f, 10.0f, 20.0f});
        label->Measure(nopeui::Size{400.0f, 200.0f});
        const nopeui::Size margined = label->DesiredSize();
        NOPEUI_CHECK(Near(margined.width, bare_width + 20.0f));

        label->Arrange(nopeui::Rect{0.0f, 0.0f, 400.0f, 200.0f});
        NOPEUI_CHECK(Near(label->Bounds().x, 10.0f));
        NOPEUI_CHECK(Near(label->Bounds().y, 20.0f));
    }

    // Explicit Width overrides the content size and travels through the
    // property table so styles can set it.
    {
        auto label = nopeui::Label::Create(L"Fixed width");
        nopeui::PropertyValue value;
        NOPEUI_CHECK(label->SetProperty(L"Width", nopeui::PropertyValue(120.0f)));
        label->Measure(nopeui::Size{400.0f, 200.0f});
        NOPEUI_CHECK(Near(label->DesiredSize().width, 120.0f));
        NOPEUI_CHECK(label->GetProperty(L"Width", &value));
        float width = 0.0f;
        NOPEUI_CHECK(value.AsFloat(&width) && Near(width, 120.0f));
    }

    // Min/Max clamp both the available slot and the resolved content size.
    {
        auto label = nopeui::Label::Create(L"Clamped");
        label->SetMinWidth(180.0f);
        label->SetMaxWidth(200.0f);
        label->Measure(nopeui::Size{500.0f, 100.0f});
        NOPEUI_CHECK(Near(label->DesiredSize().width, 180.0f));
        label->Measure(nopeui::Size{120.0f, 100.0f});
        NOPEUI_CHECK(Near(label->DesiredSize().width, 180.0f));
    }

    // Alignment resolves inside the slot: center leaves equal space on both
    // sides, right pins to the edge, stretch fills.
    {
        auto label = nopeui::Label::Create(L"Align");
        label->SetWidth(100.0f);
        label->Measure(nopeui::Size{300.0f, 100.0f});

        label->SetHorizontalAlignment(nopeui::HorizontalAlignment::Center);
        label->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 100.0f});
        NOPEUI_CHECK(Near(label->Bounds().x, 100.0f));
        NOPEUI_CHECK(Near(label->Bounds().width, 100.0f));

        label->SetHorizontalAlignment(nopeui::HorizontalAlignment::Right);
        label->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 100.0f});
        NOPEUI_CHECK(Near(label->Bounds().x, 200.0f));

        label->SetHorizontalAlignment(nopeui::HorizontalAlignment::Left);
        label->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 100.0f});
        NOPEUI_CHECK(Near(label->Bounds().x, 0.0f));

        // Stretch with an explicit width keeps that width and stays at the
        // slot start instead of drifting to the middle.
        label->SetHorizontalAlignment(nopeui::HorizontalAlignment::Stretch);
        label->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 100.0f});
        NOPEUI_CHECK(Near(label->Bounds().x, 0.0f));
        NOPEUI_CHECK(Near(label->Bounds().width, 100.0f));

        label->SetWidth(0.0f);
        label->Measure(nopeui::Size{300.0f, 100.0f});
        label->SetHorizontalAlignment(nopeui::HorizontalAlignment::Stretch);
        label->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 100.0f});
        NOPEUI_CHECK(Near(label->Bounds().width, 300.0f));
    }

    // Vertical alignment follows the same rule on the other axis.
    {
        auto label = nopeui::Label::Create(L"Vertical");
        label->Measure(nopeui::Size{300.0f, 200.0f});
        const float height = label->DesiredSize().height;
        label->SetHeight(40.0f);
        label->Measure(nopeui::Size{300.0f, 200.0f});
        NOPEUI_CHECK(height > 0.0f);

        label->SetVerticalAlignment(nopeui::VerticalAlignment::Bottom);
        label->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 200.0f});
        NOPEUI_CHECK(Near(label->Bounds().y, 160.0f));
        NOPEUI_CHECK(Near(label->Bounds().height, 40.0f));

        label->SetVerticalAlignment(nopeui::VerticalAlignment::Center);
        label->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 200.0f});
        NOPEUI_CHECK(Near(label->Bounds().y, 80.0f));

        label->SetVerticalAlignment(nopeui::VerticalAlignment::Stretch);
        label->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 200.0f});
        NOPEUI_CHECK(Near(label->Bounds().y, 0.0f));
        NOPEUI_CHECK(Near(label->Bounds().height, 40.0f));
    }

    // Attached properties: defaults, storage and unknown names.
    {
        auto button = nopeui::Button::Create(L"Cell");
        nopeui::PropertyValue value;
        NOPEUI_CHECK(button->GetAttachedProperty(L"Grid.Row", &value));
        int row = -1;
        NOPEUI_CHECK(value.AsInt(&row) && row == 0);

        NOPEUI_CHECK(button->SetAttachedProperty(L"Grid.Row", nopeui::PropertyValue(2)));
        NOPEUI_CHECK(button->SetAttachedProperty(L"Grid.ColumnSpan", nopeui::PropertyValue(3)));
        NOPEUI_CHECK(button->GetAttachedProperty(L"Grid.Row", &value));
        NOPEUI_CHECK(value.AsInt(&row) && row == 2);
        NOPEUI_CHECK(button->GetAttachedProperty(L"Grid.ColumnSpan", &value));
        NOPEUI_CHECK(value.AsInt(&row) && row == 3);

        NOPEUI_CHECK(!button->SetAttachedProperty(L"Grid.Row", nopeui::PropertyValue(L"two")));
        NOPEUI_CHECK(button->GetAttachedProperty(L"Canvas.Left", &value));
        float left = -1.0f;
        NOPEUI_CHECK(value.AsFloat(&left) && Near(left, 0.0f));
        NOPEUI_CHECK(button->SetAttachedProperty(L"Canvas.Left", nopeui::PropertyValue(1.0f)));
        NOPEUI_CHECK(!button->GetAttachedProperty(L"Lattice.Row", &value));
    }

    // Style values lose to local values in both orders.
    {
        auto sheet = nopeui::StyleSheet::Create();
        auto style = nopeui::Style::Create();
        style->SetTargetType(L"Button");
        style->Set(L"CornerRadius", nopeui::PropertyValue(9.0f));
        sheet->Add(style);

        auto local_first = nopeui::Button::Create(L"Local first");
        NOPEUI_CHECK(local_first->SetProperty(L"CornerRadius", nopeui::PropertyValue(3.0f)));
        sheet->ApplyTo(*local_first);
        nopeui::PropertyValue value;
        float radius = 0.0f;
        NOPEUI_CHECK(local_first->GetProperty(L"CornerRadius", &value));
        NOPEUI_CHECK(value.AsFloat(&radius) && Near(radius, 3.0f));

        auto style_first = nopeui::Button::Create(L"Style first");
        sheet->ApplyTo(*style_first);
        NOPEUI_CHECK(!style_first->HasLocalProperty(L"CornerRadius"));
        NOPEUI_CHECK(style_first->GetProperty(L"CornerRadius", &value));
        NOPEUI_CHECK(value.AsFloat(&radius) && Near(radius, 9.0f));
        NOPEUI_CHECK(style_first->SetProperty(L"CornerRadius", nopeui::PropertyValue(5.0f)));
        NOPEUI_CHECK(style_first->GetProperty(L"CornerRadius", &value));
        NOPEUI_CHECK(value.AsFloat(&radius) && Near(radius, 5.0f));
    }

    // Layout inputs reach styles too: a sheet can set Margin on every label.
    {
        auto sheet = nopeui::StyleSheet::Create();
        auto style = nopeui::Style::Create();
        style->SetTargetType(L"Label");
        style->Set(L"Margin", nopeui::PropertyValue(nopeui::Thickness{4.0f, 0.0f, 4.0f, 8.0f}));
        sheet->Add(style);

        auto panel = nopeui::Border::Create();
        auto label = nopeui::Label::Create(L"Styled margin");
        panel->SetChild(label);
        sheet->ApplyTo(*panel);
        NOPEUI_CHECK(Near(label->Margin().bottom, 8.0f));
        NOPEUI_CHECK(Near(label->Margin().left, 4.0f));
    }

    // ClearValue drops a local value and hands the property to the style.
    {
        auto sheet = nopeui::StyleSheet::Create();
        auto style = nopeui::Style::Create();
        style->SetTargetType(L"Button");
        style->Set(L"CornerRadius", nopeui::PropertyValue(9.0f));
        sheet->Add(style);

        auto button = nopeui::Button::Create(L"Clearable");
        NOPEUI_CHECK(button->SetProperty(L"CornerRadius", nopeui::PropertyValue(3.0f)));
        sheet->ApplyTo(*button);
        NOPEUI_CHECK(button->HasLocalProperty(L"CornerRadius"));

        NOPEUI_CHECK(button->ClearValue(L"CornerRadius"));
        NOPEUI_CHECK(!button->HasLocalProperty(L"CornerRadius"));
        nopeui::PropertyValue value;
        float radius = 0.0f;
        NOPEUI_CHECK(button->GetProperty(L"CornerRadius", &value));
        NOPEUI_CHECK(value.AsFloat(&radius) && Near(radius, 9.0f));

        NOPEUI_CHECK(!button->ClearValue(L"Missing"));

        // ClearValue also resets attached properties to their defaults.
        NOPEUI_CHECK(button->SetAttachedProperty(L"Canvas.Left", nopeui::PropertyValue(7.0f)));
        NOPEUI_CHECK(button->ClearValue(L"Canvas.Left"));
        NOPEUI_CHECK(button->GetAttachedProperty(L"Canvas.Left", &value));
        float left = -1.0f;
        NOPEUI_CHECK(value.AsFloat(&left) && Near(left, 0.0f));
    }

    return 0;
}
