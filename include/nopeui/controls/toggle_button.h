#ifndef NOPEUI_CONTROLS_TOGGLE_BUTTON_H_
#define NOPEUI_CONTROLS_TOGGLE_BUTTON_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {
namespace graphics {
class TextRun;
}

// Button that keeps a checked state. Click, Enter or Space toggles it.
class ToggleButton : public Element {
public:
    static Ref<ToggleButton> Create(std::wstring text) noexcept;

    bool IsChecked() const noexcept {
        return checked_;
    }
    void SetChecked(bool checked) noexcept;

    void SetOnCheckedChanged(std::function<void(bool)> handler) {
        checked_changed_ = std::move(handler);
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    void OnPointerEnter() noexcept override;
    void OnPointerLeave() noexcept override;
    void OnPointerDown(Point point) noexcept override;
    void OnPointerUp(Point point) noexcept override;
    bool IsFocusable() const noexcept override;
    bool OnKeyDown(unsigned int key) noexcept override;
    bool OnKeyUp(unsigned int key) noexcept override;

private:
    ToggleButton() noexcept = default;

    void Toggle() noexcept;

    std::wstring text_;
    std::unique_ptr<graphics::TextRun> run_;
    std::function<void(bool)> checked_changed_;
    bool checked_ = false;
    bool hovered_ = false;
    bool pressed_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_TOGGLE_BUTTON_H_
