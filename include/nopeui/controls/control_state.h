#ifndef NOPEUI_CONTROLS_CONTROL_STATE_H_
#define NOPEUI_CONTROLS_CONTROL_STATE_H_

namespace nopeui {

enum class ControlState {
    Normal,
    Hovered,
    Pressed,
    Disabled,
};

constexpr ControlState ResolveControlState(
    bool enabled,
    bool hovered,
    bool pressed) noexcept {
    if (!enabled) {
        return ControlState::Disabled;
    }
    if (pressed) {
        return ControlState::Pressed;
    }
    if (hovered) {
        return ControlState::Hovered;
    }
    return ControlState::Normal;
}

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_CONTROL_STATE_H_
