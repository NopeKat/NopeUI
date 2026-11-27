#ifndef NOPEUI_SAMPLES_PANELS_PAGE_H_
#define NOPEUI_SAMPLES_PANELS_PAGE_H_

#include <string>
#include <vector>

#include "nopeui/base/geometry.h"
#include "nopeui/base/property_value.h"
#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/canvas.h"
#include "nopeui/controls/dock_panel.h"
#include "nopeui/controls/flex_panel.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/layout/attached.h"
#include "nopeui/layout/layout_types.h"
#include "nopeui/layout/style.h"

namespace nopeui::samples {

// Builds one titled card around the given panel so the page reads as three
// sections. Shared by the golden tool, which pins the panel geometry in pixels.
inline Ref<Border> MakePanelsCard(std::wstring title, Ref<Element> body) {
    auto card = Border::Create();
    card->UseSurfaceBackground(true);
    card->SetCornerRadius(10.0f);
    card->SetPadding(Thickness{16.0f, 16.0f, 16.0f, 16.0f});
    auto content = StackPanel::Create(10.0f);
    content->SetOrientation(Orientation::Vertical);
    content->Add(Label::Create(std::move(title)));
    content->Add(std::move(body));
    card->SetChild(content);
    return card;
}

// Panel showcase shared by the golden tool: Canvas attached offsets, Dock edges
// with the last child filling, and FlexPanel grow and wrap in one page.
inline Ref<Element> BuildPanelsPage() {
    auto page = Border::Create();
    page->SetBorderVisible(false);
    page->SetPadding(Thickness{24.0f, 24.0f, 24.0f, 24.0f});

    auto stack = StackPanel::Create(16.0f);
    stack->SetOrientation(Orientation::Vertical);

    auto canvas = Canvas::Create();
    canvas->SetHeight(140.0f);
    struct Chip {
        std::wstring text;
        float left;
        float top;
        float width;
    };
    const std::vector<Chip> chips = {
        {L"Left 0, Top 0", 0.0f, 0.0f, 150.0f},
        {L"Left 170, Top 24", 170.0f, 24.0f, 170.0f},
        {L"Left 60, Top 84", 60.0f, 84.0f, 150.0f},
    };
    for (const Chip& chip : chips) {
        auto button = Button::Create(chip.text);
        button->SetWidth(chip.width);
        button->SetHeight(36.0f);
        button->SetAttachedProperty(L"Canvas.Left", PropertyValue(chip.left));
        button->SetAttachedProperty(L"Canvas.Top", PropertyValue(chip.top));
        canvas->Add(button);
    }
    stack->Add(MakePanelsCard(L"Canvas: absolute offsets", canvas));

    auto dock = DockPanel::Create();
    dock->SetHeight(150.0f);
    auto edge = [](std::wstring text, int side) {
        auto label = Label::Create(std::move(text));
        // 0 Left, 1 Top, 2 Right, 3 Bottom.
        label->SetAttachedProperty(L"Dock", PropertyValue(side));
        return label;
    };
    dock->Add(edge(L"Top edge", 1));
    dock->Add(edge(L"Bottom edge", 3));
    dock->Add(edge(L"Left edge", 0));
    auto fill = Border::Create();
    fill->UseSurfaceBackground(true);
    fill->SetCornerRadius(6.0f);
    fill->SetPadding(Thickness{12.0f, 8.0f, 12.0f, 8.0f});
    fill->SetChild(Label::Create(L"Fill child"));
    dock->Add(fill);
    stack->Add(MakePanelsCard(L"DockPanel: three edges, fill last", dock));

    auto flex_content = StackPanel::Create(10.0f);
    flex_content->SetOrientation(Orientation::Vertical);
    auto row = FlexPanel::Create(8.0f);
    row->SetHeight(44.0f);
    auto grow = [](std::wstring text, float weight) {
        auto button = Button::Create(std::move(text));
        button->SetAttachedProperty(L"FlexGrow", PropertyValue(weight));
        return button;
    };
    row->Add(grow(L"Grow 1", 1.0f));
    row->Add(grow(L"Grow 2", 2.0f));
    row->Add(grow(L"Grow 1", 1.0f));
    flex_content->Add(row);

    auto wrapped = FlexPanel::Create(8.0f);
    wrapped->SetWrap(true);
    for (int index = 0; index < 8; ++index) {
        std::wstring text = L"Tag 0";
        text.back() = static_cast<wchar_t>(L'0' + index);
        auto button = Button::Create(std::move(text));
        button->SetWidth(140.0f);
        button->SetHeight(34.0f);
        wrapped->Add(button);
    }
    flex_content->Add(wrapped);
    stack->Add(MakePanelsCard(L"FlexPanel: grow and wrap", flex_content));

    page->SetChild(stack);
    return page;
}

inline Ref<StyleSheet> BuildPanelsStyleSheet() {
    auto sheet = StyleSheet::Create();
    auto button = Style::Create();
    button->SetTargetType(L"Button");
    button->Set(L"CornerRadius", PropertyValue(6.0f));
    sheet->Add(button);
    return sheet;
}

}  // namespace nopeui::samples

#endif  // NOPEUI_SAMPLES_PANELS_PAGE_H_
