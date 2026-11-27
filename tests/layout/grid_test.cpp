#include "../test_check.h"

#include <cmath>

#include "nopeui/controls/grid.h"
#include "nopeui/controls/label.h"
#include "nopeui/layout/attached.h"
#include "nopeui/layout/layout_types.h"

namespace {

bool Near(float left, float right) {
    return std::fabs(left - right) < 1.0f;
}

void Place(nopeui::Element& child, int row, int column) {
    child.SetAttachedProperty(nopeui::GridRowProperty(), nopeui::PropertyValue(row));
    child.SetAttachedProperty(nopeui::GridColumnProperty(), nopeui::PropertyValue(column));
}

}  // namespace

int main() {
    // A pixel column keeps its size and the star column takes the rest.
    {
        auto grid = nopeui::Grid::Create();
        grid->SetColumns({nopeui::PixelLength(100.0f), nopeui::StarLength(1.0f)});
        grid->SetRows({nopeui::StarLength(1.0f)});

        auto left = nopeui::Label::Create(L"Left");
        auto right = nopeui::Label::Create(L"Right");
        Place(*left, 0, 0);
        Place(*right, 0, 1);
        grid->Add(left);
        grid->Add(right);

        grid->Measure(nopeui::Size{300.0f, 120.0f});
        grid->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 120.0f});

        NOPEUI_CHECK(Near(left->Bounds().x, 0.0f));
        NOPEUI_CHECK(Near(left->Bounds().width, 100.0f));
        NOPEUI_CHECK(Near(right->Bounds().x, 100.0f));
        NOPEUI_CHECK(Near(right->Bounds().width, 200.0f));
    }

    // Row heights: pixel then star, with spacing between them.
    {
        auto grid = nopeui::Grid::Create();
        grid->SetColumns({nopeui::StarLength(1.0f)});
        grid->SetRows({nopeui::PixelLength(40.0f), nopeui::StarLength(1.0f)});
        grid->SetRowSpacing(10.0f);

        auto top = nopeui::Label::Create(L"Top");
        auto bottom = nopeui::Label::Create(L"Bottom");
        top->SetAttachedProperty(nopeui::GridRowProperty(), nopeui::PropertyValue(0));
        bottom->SetAttachedProperty(nopeui::GridRowProperty(), nopeui::PropertyValue(1));
        grid->Add(top);
        grid->Add(bottom);

        grid->Measure(nopeui::Size{200.0f, 110.0f});
        grid->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 110.0f});

        NOPEUI_CHECK(Near(top->Bounds().height, 40.0f));
        NOPEUI_CHECK(Near(bottom->Bounds().y, 50.0f));
        NOPEUI_CHECK(Near(bottom->Bounds().height, 60.0f));
    }

    // Auto columns size to the widest child.
    {
        auto grid = nopeui::Grid::Create();
        grid->SetColumns({nopeui::AutoLength()});
        grid->SetRows({nopeui::AutoLength()});
        auto label = nopeui::Label::Create(L"Auto sized");
        grid->Add(label);

        label->Measure(nopeui::Size{400.0f, 100.0f});
        const float natural = label->DesiredSize().width;
        grid->Measure(nopeui::Size{400.0f, 100.0f});
        NOPEUI_CHECK(Near(grid->DesiredSize().width, natural));
        grid->Arrange(nopeui::Rect{0.0f, 0.0f, grid->DesiredSize().width, 100.0f});
        NOPEUI_CHECK(Near(label->Bounds().width, natural));
    }

    // Spans: column span adds the tracks it covers; row span adds heights.
    {
        auto grid = nopeui::Grid::Create();
        grid->SetColumns(
            {nopeui::PixelLength(60.0f), nopeui::PixelLength(60.0f), nopeui::StarLength(1.0f)});
        grid->SetRows({nopeui::PixelLength(30.0f), nopeui::PixelLength(30.0f)});
        grid->SetColumnSpacing(10.0f);

        auto wide = nopeui::Label::Create(L"Wide");
        wide->SetAttachedProperty(nopeui::GridColumnProperty(), nopeui::PropertyValue(1));
        wide->SetAttachedProperty(nopeui::GridColumnSpanProperty(), nopeui::PropertyValue(2));
        auto tall = nopeui::Label::Create(L"Tall");
        tall->SetAttachedProperty(nopeui::GridRowSpanProperty(), nopeui::PropertyValue(2));
        grid->Add(wide);
        grid->Add(tall);

        grid->Measure(nopeui::Size{300.0f, 100.0f});
        grid->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 100.0f});

        NOPEUI_CHECK(Near(wide->Bounds().x, 70.0f));
        NOPEUI_CHECK(Near(wide->Bounds().width, 60.0f + 10.0f + 160.0f));
        NOPEUI_CHECK(Near(tall->Bounds().height, 60.0f));
        NOPEUI_CHECK(Near(tall->Bounds().y, 0.0f));
    }

    // Star tracks collapse to content size on an unbounded axis, which is what
    // a vertical ScrollViewer offers.
    {
        auto grid = nopeui::Grid::Create();
        grid->SetColumns({nopeui::StarLength(1.0f)});
        grid->SetRows({nopeui::StarLength(1.0f), nopeui::StarLength(2.0f)});
        auto first = nopeui::Label::Create(L"First");
        auto second = nopeui::Label::Create(L"Second");
        second->SetAttachedProperty(nopeui::GridRowProperty(), nopeui::PropertyValue(1));
        grid->Add(first);
        grid->Add(second);

        grid->Measure(nopeui::Size{200.0f, 1.0e6f});
        NOPEUI_CHECK(grid->DesiredSize().height < 200.0f);
    }

    // Margins inside a cell inset the child, not the cell.
    {
        auto grid = nopeui::Grid::Create();
        grid->SetColumns({nopeui::StarLength(1.0f)});
        grid->SetRows({nopeui::StarLength(1.0f)});
        auto label = nopeui::Label::Create(L"Margin");
        label->SetMargin(nopeui::Thickness{12.0f, 8.0f, 12.0f, 8.0f});
        grid->Add(label);

        grid->Measure(nopeui::Size{200.0f, 100.0f});
        grid->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 100.0f});
        NOPEUI_CHECK(Near(label->Bounds().x, 12.0f));
        NOPEUI_CHECK(Near(label->Bounds().y, 8.0f));
        NOPEUI_CHECK(Near(label->Bounds().width, 176.0f));
        NOPEUI_CHECK(Near(label->Bounds().height, 84.0f));
    }

    return 0;
}
