#ifndef NOPEUI_BASE_PROPERTY_H_
#define NOPEUI_BASE_PROPERTY_H_

namespace nopeui {

// Minimal value slot with change detection. Set() returns whether the value
// actually changed so the owner can decide to invalidate and repaint.
template <typename T>
class Property {
public:
    Property() = default;
    explicit Property(const T& value) noexcept : value_(value) {}

    const T& Get() const noexcept {
        return value_;
    }

    bool Set(T value) {
        if (value_ == value) {
            return false;
        }
        value_ = value;
        return true;
    }

private:
    T value_{};
};

}  // namespace nopeui

#endif  // NOPEUI_BASE_PROPERTY_H_
