#ifndef NOPEUI_CONTROLS_LABEL_H_
#define NOPEUI_CONTROLS_LABEL_H_

#include <cstdint>
#include <memory>
#include <string>

#include "nopeui/base/property.h"
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

    // Zero means the theme text color.
    void SetColor(std::uint32_t color) noexcept;

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;

private:
    Label() noexcept = default;

    std::wstring text_;
    std::unique_ptr<graphics::TextRun> run_;
    Property<std::uint32_t> color_{0};
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_LABEL_H_
