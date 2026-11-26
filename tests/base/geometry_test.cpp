#include "../test_check.h"

#include "nopeui/base/geometry.h"

int main() {
    const nopeui::Point point{1.0f, 2.0f};
    NOPEUI_CHECK(point.x == 1.0f);
    NOPEUI_CHECK(point.y == 2.0f);

    const nopeui::Size size{3.0f, 4.0f};
    NOPEUI_CHECK(size.width == 3.0f);
    NOPEUI_CHECK(size.height == 4.0f);

    const nopeui::Rect rect{1.0f, 2.0f, 3.0f, 4.0f};
    NOPEUI_CHECK(rect.Right() == 4.0f);
    NOPEUI_CHECK(rect.Bottom() == 6.0f);
    NOPEUI_CHECK(!rect.IsEmpty());

    const nopeui::Rect empty{1.0f, 2.0f, 0.0f, 4.0f};
    NOPEUI_CHECK(empty.IsEmpty());

    const nopeui::Thickness thickness{1.0f, 2.0f, 3.0f, 4.0f};
    NOPEUI_CHECK(thickness.Horizontal() == 4.0f);
    NOPEUI_CHECK(thickness.Vertical() == 6.0f);

    return 0;
}
