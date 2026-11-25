#include <cassert>

#include "nopeui/base/geometry.h"

int main() {
    const nopeui::Point point{1.0f, 2.0f};
    assert(point.x == 1.0f);
    assert(point.y == 2.0f);

    const nopeui::Size size{3.0f, 4.0f};
    assert(size.width == 3.0f);
    assert(size.height == 4.0f);

    const nopeui::Rect rect{1.0f, 2.0f, 3.0f, 4.0f};
    assert(rect.Right() == 4.0f);
    assert(rect.Bottom() == 6.0f);
    assert(!rect.IsEmpty());

    const nopeui::Rect empty{1.0f, 2.0f, 0.0f, 4.0f};
    assert(empty.IsEmpty());

    const nopeui::Thickness thickness{1.0f, 2.0f, 3.0f, 4.0f};
    assert(thickness.Horizontal() == 4.0f);
    assert(thickness.Vertical() == 6.0f);

    return 0;
}
