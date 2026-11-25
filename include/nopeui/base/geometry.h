#ifndef NOPEUI_BASE_GEOMETRY_H_
#define NOPEUI_BASE_GEOMETRY_H_

namespace nopeui {

struct Point {
    float x = 0.0f;
    float y = 0.0f;

    friend bool operator==(const Point&, const Point&) = default;
};

struct Size {
    float width = 0.0f;
    float height = 0.0f;

    friend bool operator==(const Size&, const Size&) = default;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    float Right() const noexcept {
        return x + width;
    }

    float Bottom() const noexcept {
        return y + height;
    }

    bool IsEmpty() const noexcept {
        return width <= 0.0f || height <= 0.0f;
    }

    friend bool operator==(const Rect&, const Rect&) = default;
};

struct Thickness {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    float Horizontal() const noexcept {
        return left + right;
    }

    float Vertical() const noexcept {
        return top + bottom;
    }

    friend bool operator==(const Thickness&, const Thickness&) = default;
};

}  // namespace nopeui

#endif  // NOPEUI_BASE_GEOMETRY_H_
