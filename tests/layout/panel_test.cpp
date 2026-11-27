#include "../test_check.h"

#include <cmath>

#include "nopeui/controls/border.h"
#include "nopeui/controls/canvas.h"
#include "nopeui/controls/dock_panel.h"
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

    return 0;
}
