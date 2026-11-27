#ifndef NOPEUI_LAYOUT_LAYOUT_TYPES_H_
#define NOPEUI_LAYOUT_LAYOUT_TYPES_H_

namespace nopeui {

// How an element fills the slot its parent hands it. Stretch is the default,
// matching the usual desktop layout rule; an explicit Width or Height turns
// Stretch into the matching non-stretch behavior once the constraint clamps.
enum class HorizontalAlignment {
    Left,
    Center,
    Right,
    Stretch,
};

enum class VerticalAlignment {
    Top,
    Center,
    Bottom,
    Stretch,
};

// Grid track sizing. Pixel lengths are fixed, Auto asks the content, Star
// divides the remaining space by weight.
enum class GridUnitType {
    Pixel,
    Auto,
    Star,
};

struct GridLength {
    float value = 0.0f;
    GridUnitType unit = GridUnitType::Star;

    float Weight() const noexcept {
        return value > 0.0f ? value : 1.0f;
    }

    friend bool operator==(const GridLength&, const GridLength&) = default;
};

inline GridLength PixelLength(float value) noexcept {
    return GridLength{value, GridUnitType::Pixel};
}

inline GridLength AutoLength() noexcept {
    return GridLength{0.0f, GridUnitType::Auto};
}

inline GridLength StarLength(float weight = 1.0f) noexcept {
    return GridLength{weight, GridUnitType::Star};
}

}  // namespace nopeui

#endif  // NOPEUI_LAYOUT_LAYOUT_TYPES_H_
