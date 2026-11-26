#ifndef NOPEUI_CONTROLS_BUTTON_H_
#define NOPEUI_CONTROLS_BUTTON_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "nopeui/base/event.h"
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

    Subscription SubscribeClick(Event<>::Handler handler) {
        return click_.Subscribe(std::move(handler));
    }

    // Keeps the subscription inside the button for the common single handler case.
    void SetOnClick(Event<>::Handler handler) {
        click_subscription_.Reset();
        click_subscription_ = click_.Subscribe(std::move(handler));
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
    Button() noexcept = default;

    std::wstring text_;
    std::unique_ptr<graphics::TextRun> run_;
    Event<> click_;
    Subscription click_subscription_;
    bool hovered_ = false;
    bool pressed_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_BUTTON_H_
