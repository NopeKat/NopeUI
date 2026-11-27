#ifndef NOPEUI_SAMPLES_FORM_PAGE_H_
#define NOPEUI_SAMPLES_FORM_PAGE_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "nopeui/base/geometry.h"
#include "nopeui/base/property_value.h"
#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/check_box.h"
#include "nopeui/controls/grid.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/progress_bar.h"
#include "nopeui/controls/radio_button.h"
#include "nopeui/controls/slider.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/controls/text_box.h"
#include "nopeui/controls/toggle_switch.h"
#include "nopeui/layout/attached.h"
#include "nopeui/layout/layout_types.h"
#include "nopeui/layout/style.h"
#include "nopeui/theme/control_template.h"

namespace nopeui::samples {

// One form page shared by the smoke app and the golden tool, so the picture the
// user sees is the picture the baselines pin down. It exists to exercise Grid
// tracks, attached cells, margins, alignment and spanning labels in one tree.
inline Ref<Element> BuildFormPage(std::function<void(bool dark)> on_theme = {}) {
    auto page = Border::Create();
    page->SetBorderVisible(false);
    page->SetPadding(Thickness{24.0f, 24.0f, 24.0f, 24.0f});

    auto card = Border::Create();
    card->UseSurfaceBackground(true);
    card->SetCornerRadius(10.0f);
    card->SetPadding(Thickness{28.0f, 24.0f, 28.0f, 24.0f});

    auto grid = Grid::Create();
    grid->SetColumns({AutoLength(), StarLength(1.0f)});
    grid->SetColumnSpacing(16.0f);
    grid->SetRowSpacing(10.0f);
    // Ten auto rows: header, subtitle, four fields, two options, progress and
    // the action bar. Auto keeps the form compact when the window is tall.
    for (int index = 0; index < 10; ++index) {
        grid->AddRow(AutoLength());
    }

    auto place = [](Element& element, int row, int column, int column_span = 1) {
        element.SetAttachedProperty(GridRowProperty(), PropertyValue(row));
        element.SetAttachedProperty(GridColumnProperty(), PropertyValue(column));
        if (column_span > 1) {
            element.SetAttachedProperty(
                GridColumnSpanProperty(), PropertyValue(column_span));
        }
    };

    auto header = Label::Create(L"NopeUI form");
    header->SetMargin(Thickness{0.0f, 0.0f, 0.0f, 4.0f});
    place(*header, 0, 0, 2);
    grid->Add(header);

    auto subtitle = Label::Create(
        L"Grid tracks, margins, alignment and attached cells in one page");
    place(*subtitle, 1, 0, 2);
    grid->Add(subtitle);

    auto field_label = [](const wchar_t* text) {
        auto label = Label::Create(text);
        label->SetMinWidth(96.0f);
        label->SetVerticalAlignment(VerticalAlignment::Center);
        return label;
    };

    auto name_label = field_label(L"Name");
    place(*name_label, 2, 0);
    grid->Add(name_label);
    auto name_box = TextBox::Create(L"Ada Lovelace");
    place(*name_box, 2, 1);
    grid->Add(name_box);

    auto mail_label = field_label(L"Email");
    place(*mail_label, 3, 0);
    grid->Add(mail_label);
    auto mail_box = TextBox::Create(L"ada@example.com");
    place(*mail_box, 3, 1);
    grid->Add(mail_box);

    auto role_label = field_label(L"Role");
    place(*role_label, 4, 0);
    grid->Add(role_label);
    auto roles = StackPanel::Create(18.0f);
    roles->SetOrientation(Orientation::Horizontal);
    auto owner = RadioButton::Create(L"Owner");
    owner->SetChecked(true);
    roles->Add(owner);
    roles->Add(RadioButton::Create(L"Editor"));
    roles->Add(RadioButton::Create(L"Viewer"));
    place(*roles, 4, 1);
    grid->Add(roles);

    auto plan_label = field_label(L"Plan");
    place(*plan_label, 5, 0);
    grid->Add(plan_label);
    auto plans = StackPanel::Create(18.0f);
    plans->SetOrientation(Orientation::Horizontal);
    auto yearly = CheckBox::Create(L"Yearly billing");
    yearly->SetChecked(true);
    plans->Add(yearly);
    auto notify = ToggleSwitch::Create(L"Email updates");
    notify->SetChecked(true);
    plans->Add(notify);
    place(*plans, 5, 1);
    grid->Add(plans);

    auto volume_label = field_label(L"Volume");
    place(*volume_label, 6, 0);
    grid->Add(volume_label);
    auto volume = Slider::Create(60.0, 0.0, 100.0);
    place(*volume, 6, 1);
    grid->Add(volume);

    auto quota_label = field_label(L"Quota");
    place(*quota_label, 7, 0);
    grid->Add(quota_label);
    auto quota = ProgressBar::Create(0.72f);
    place(*quota, 7, 1);
    grid->Add(quota);

    auto hint = Label::Create(
        L"Labels sit in the auto column and keep their width; fields stretch in "
        L"the star column. Everything below is one Grid.");
    hint->SetMargin(Thickness{0.0f, 8.0f, 0.0f, 0.0f});
    place(*hint, 8, 0, 2);
    grid->Add(hint);

    auto actions = StackPanel::Create(10.0f);
    actions->SetOrientation(Orientation::Horizontal);
    auto save = Button::Create(L"Save");
    save->SetKey(L"primary");
    actions->Add(save);
    actions->Add(Button::Create(L"Cancel"));
    if (on_theme) {
        auto light = Button::Create(L"Light");
        light->SetOnClick([on_theme] { on_theme(false); });
        auto dark = Button::Create(L"Dark");
        dark->SetOnClick([on_theme] { on_theme(true); });
        actions->Add(light);
        actions->Add(dark);
    }
    place(*actions, 9, 0, 2);
    grid->Add(actions);

    card->SetChild(grid);
    page->SetChild(card);
    return page;
}

// Primary button palette used by both samples; kept next to the page so the
// golden image and the running window share one look.
inline Ref<StyleSheet> BuildFormStyleSheet() {
    auto style = Style::Create();
    style->SetTargetType(L"Button");
    style->SetTargetKey(L"primary");
    style->Set(L"CornerRadius", PropertyValue(6.0f));

    auto control_template = ControlTemplate::Create();
    TemplatePart background;
    background.background = 0x0067C0u;
    background.border = 0x005A9Eu;
    background.corner_radius = 6.0f;
    control_template->SetPart(L"Background", background);
    TemplatePart content;
    content.foreground = 0xFFFFFFu;
    control_template->SetPart(L"Content", content);
    style->SetTemplate(control_template);

    auto sheet = StyleSheet::Create();
    sheet->Add(style);
    return sheet;
}

}  // namespace nopeui::samples

#endif  // NOPEUI_SAMPLES_FORM_PAGE_H_
