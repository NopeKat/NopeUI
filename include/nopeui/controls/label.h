#ifndef NOPEUI_CONTROLS_LABEL_H_
#define NOPEUI_CONTROLS_LABEL_H_

#include <memory>
#include <string>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {
namespace graphics {
class TextRun;
}

// Single line of text.
class Label : public Element {
public:
    static Ref<Label> Create(std::wstring text) noexcept;

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;

private:
    Label() noexcept = default;

    std::wstring text_;
    std::unique_ptr<graphics::TextRun> run_;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_LABEL_H_
