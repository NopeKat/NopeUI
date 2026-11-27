#include "../test_check.h"

#include <cmath>

#include "nopeui/controls/border.h"
#include "nopeui/controls/canvas.h"
#include "nopeui/controls/dock_panel.h"
#include "nopeui/controls/flex_panel.h"
#include "nopeui/layout/attached.h"
#include "nopeui/layout/layout_types.h"

namespace {

bool Near(float left, float right) {
    return std::fabs(left - right) < 1.0f;
}

nopeui::Ref<nopeui::Border> Box(float width, float height) {
    auto border = nopeui::Border::Create();
    border->SetWidth(width);
    border->SetHeight(height);
    return border;
}

void Dock(nopeui::Element& child, int side) {
    child.SetAttachedProperty(nopeui::DockProperty(), nopeui::PropertyValue(side));
}

}  // namespace

int main() {
    // Canvas places a child at its offset and sizes itself to the far edge.
    {
        auto canvas = nopeui::Canvas::Create();
        auto box = Box(40.0f, 20.0f);
        box->SetAttachedProperty(
            nopeui::CanvasLeftProperty(), nopeui::PropertyValue(30.0f));
        box->SetAttachedProperty(
            nopeui::CanvasTopProperty(), nopeui::PropertyValue(10.0f));
        canvas->Add(box);

        canvas->Measure(nopeui::Size{200.0f, 100.0f});
        canvas->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 100.0f});

        NOPEUI_CHECK(Near(box->Bounds().x, 30.0f));
        NOPEUI_CHECK(Near(box->Bounds().y, 10.0f));
        NOPEUI_CHECK(Near(box->Bounds().width, 40.0f));
        NOPEUI_CHECK(Near(canvas->DesiredSize().width, 70.0f));
        NOPEUI_CHECK(Near(canvas->DesiredSize().height, 30.0f));
    }

    // A canvas nested away from the origin adds its own bounds, so offsets stay
    // relative to the panel instead of the window.
    {
        auto canvas = nopeui::Canvas::Create();
        auto box = Box(40.0f, 20.0f);
        box->SetAttachedProperty(
            nopeui::CanvasLeftProperty(), nopeui::PropertyValue(30.0f));
        box->SetAttachedProperty(
            nopeui::CanvasTopProperty(), nopeui::PropertyValue(10.0f));
        canvas->Add(box);

        canvas->Measure(nopeui::Size{200.0f, 100.0f});
        canvas->Arrange(nopeui::Rect{25.0f, 40.0f, 200.0f, 100.0f});

        NOPEUI_CHECK(Near(box->Bounds().x, 55.0f));
        NOPEUI_CHECK(Near(box->Bounds().y, 50.0f));
    }

    // Overlapping children stack in paint order, so the last one wins a hit.
    {
        auto canvas = nopeui::Canvas::Create();
        auto bottom = Box(50.0f, 50.0f);
        auto top = Box(50.0f, 50.0f);
        top->SetAttachedProperty(
            nopeui::CanvasLeftProperty(), nopeui::PropertyValue(10.0f));
        top->SetAttachedProperty(
            nopeui::CanvasTopProperty(), nopeui::PropertyValue(10.0f));
        canvas->Add(bottom);
        canvas->Add(top);

        canvas->Measure(nopeui::Size{100.0f, 100.0f});
        canvas->Arrange(nopeui::Rect{0.0f, 0.0f, 100.0f, 100.0f});

        nopeui::Element* hit = canvas->HitTest(nopeui::Point{20.0f, 20.0f});
        NOPEUI_CHECK(hit == top.Get());
        NOPEUI_CHECK(top->Bounds().x >= 0.0f);
        NOPEUI_CHECK(Near(top->Bounds().width, 50.0f));
    }

    // DockPanel claims edges in order and the last child fills the rest.
    {
        auto panel = nopeui::DockPanel::Create();
        auto top = Box(0.0f, 30.0f);
        auto left = Box(60.0f, 0.0f);
        auto fill = Box(0.0f, 0.0f);
        Dock(*top, 1);
        Dock(*left, 0);
        panel->Add(top);
        panel->Add(left);
        panel->Add(fill);

        panel->Measure(nopeui::Size{300.0f, 200.0f});
        panel->Arrange(nopeui::Rect{0.0f, 0.0f, 300.0f, 200.0f});

        NOPEUI_CHECK(Near(top->Bounds().y, 0.0f));
        NOPEUI_CHECK(Near(top->Bounds().height, 30.0f));
        NOPEUI_CHECK(Near(top->Bounds().width, 300.0f));
        NOPEUI_CHECK(Near(left->Bounds().y, 30.0f));
        NOPEUI_CHECK(Near(left->Bounds().width, 60.0f));
        NOPEUI_CHECK(Near(left->Bounds().height, 170.0f));
        NOPEUI_CHECK(Near(fill->Bounds().x, 60.0f));
        NOPEUI_CHECK(Near(fill->Bounds().y, 30.0f));
        NOPEUI_CHECK(Near(fill->Bounds().width, 240.0f));
        NOPEUI_CHECK(Near(fill->Bounds().height, 170.0f));
    }

    // Right and bottom edges come after the first two.
    {
        auto panel = nopeui::DockPanel::Create();
        panel->SetLastChildFill(false);
        auto left = Box(40.0f, 0.0f);
        auto right = Box(50.0f, 0.0f);
        auto bottom = Box(0.0f, 20.0f);
        Dock(*left, 0);
        Dock(*right, 2);
        Dock(*bottom, 3);
        panel->Add(left);
        panel->Add(right);
        panel->Add(bottom);

        panel->Measure(nopeui::Size{200.0f, 100.0f});
        panel->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 100.0f});

        NOPEUI_CHECK(Near(left->Bounds().x, 0.0f));
        NOPEUI_CHECK(Near(left->Bounds().width, 40.0f));
        NOPEUI_CHECK(Near(right->Bounds().x, 150.0f));
        NOPEUI_CHECK(Near(right->Bounds().width, 50.0f));
        NOPEUI_CHECK(Near(bottom->Bounds().y, 80.0f));
        NOPEUI_CHECK(Near(bottom->Bounds().height, 20.0f));
    }

    // FlexPanel runs along the main axis and inserts the gap between children.
    {
        auto panel = nopeui::FlexPanel::Create(10.0f);
        auto first = Box(40.0f, 20.0f);
        auto second = Box(60.0f, 30.0f);
        panel->Add(first);
        panel->Add(second);

        panel->Measure(nopeui::Size{200.0f, 100.0f});
        NOPEUI_CHECK(Near(panel->DesiredSize().width, 110.0f));
        NOPEUI_CHECK(Near(panel->DesiredSize().height, 30.0f));

        panel->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 100.0f});
        NOPEUI_CHECK(Near(first->Bounds().x, 0.0f));
        NOPEUI_CHECK(Near(second->Bounds().x, 50.0f));
        NOPEUI_CHECK(Near(second->Bounds().height, 30.0f));

        panel->SetDirection(nopeui::FlexDirection::Column);
        panel->Measure(nopeui::Size{200.0f, 100.0f});
        panel->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 100.0f});
        NOPEUI_CHECK(Near(first->Bounds().y, 0.0f));
        NOPEUI_CHECK(Near(second->Bounds().y, 30.0f));
    }

    // FlexGrow splits the leftover main-axis space by weight.
    {
        auto panel = nopeui::FlexPanel::Create(0.0f);
        auto first = Box(0.0f, 20.0f);
        auto second = Box(0.0f, 20.0f);
        first->SetAttachedProperty(
            nopeui::FlexGrowProperty(), nopeui::PropertyValue(1.0f));
        second->SetAttachedProperty(
            nopeui::FlexGrowProperty(), nopeui::PropertyValue(2.0f));
        panel->Add(first);
        panel->Add(second);

        panel->Measure(nopeui::Size{200.0f, 100.0f});
        panel->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 100.0f});
        NOPEUI_CHECK(Near(first->Bounds().width, 66.0f) || Near(first->Bounds().width, 67.0f));
        NOPEUI_CHECK(Near(second->Bounds().x, 66.0f) || Near(second->Bounds().x, 67.0f));
        NOPEUI_CHECK(Near(second->Bounds().width, 133.0f) || Near(second->Bounds().width, 134.0f));
    }

    // Wrap starts a new line once the next child no longer fits.
    {
        auto panel = nopeui::FlexPanel::Create(10.0f);
        panel->SetWrap(true);
        auto first = Box(70.0f, 20.0f);
        auto second = Box(70.0f, 20.0f);
        auto third = Box(70.0f, 20.0f);
        panel->Add(first);
        panel->Add(second);
        panel->Add(third);

        panel->Measure(nopeui::Size{200.0f, 100.0f});
        NOPEUI_CHECK(Near(panel->DesiredSize().height, 50.0f));
        panel->Arrange(nopeui::Rect{0.0f, 0.0f, 200.0f, 200.0f});
        NOPEUI_CHECK(Near(second->Bounds().x, 80.0f));
        NOPEUI_CHECK(Near(third->Bounds().y, 30.0f));
        NOPEUI_CHECK(Near(third->Bounds().x, 0.0f));
    }

    return 0;
}
