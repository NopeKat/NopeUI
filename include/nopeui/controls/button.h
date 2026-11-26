#ifndef NOPEUI_CONTROLS_BUTTON_H_
#define NOPEUI_CONTROLS_BUTTON_H_

#include <cstdint>
#include <memory>
#include <string>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {
namespace graphics {
class TextRun;
}

// Clickable text button with normal, pointer over and pressed backgrounds.
class Button : public Element {
public:
    static Ref<Button> Create(std::wstring text) noexcept;

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    void OnPointerEnter() noexcept override;
    void OnPointerLeave() noexcept override;
    void OnPointerDown(Point point) noexcept override;
    void OnPointerUp(Point point) noexcept override;

private:
    Button() noexcept = default;

    std::wstring text_;
    std::unique_ptr<graphics::TextRun> run_;
    bool hovered_ = false;
    bool pressed_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_BUTTON_H_
