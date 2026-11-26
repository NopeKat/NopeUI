#ifndef NOPEUI_CONTROLS_CONTENT_PRESENTER_H_
#define NOPEUI_CONTROLS_CONTENT_PRESENTER_H_

#include <memory>
#include <string>
#include <string_view>

#include "nopeui/base/ref.h"
#include "nopeui/layout/element.h"

namespace nopeui {
namespace graphics {
class TextRun;
}

// Shows the text content of the templated control. A ContentPresenter whose
// Text ends up empty is filled from the templated control's content.
class ContentPresenter : public Element {
public:
    static Ref<ContentPresenter> Create() noexcept;

    void SetText(std::wstring text) noexcept;
    std::wstring_view Text() const noexcept {
        return text_;
    }

    std::wstring_view TypeName() const noexcept override {
        return L"ContentPresenter";
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    const PropertyMetadata* ControlPropertyTable(std::size_t* count) const noexcept override;
    bool GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept override;
    bool SetControlProperty(std::wstring_view name, const PropertyValue& value) noexcept override;

private:
    ContentPresenter() noexcept = default;

    void RebuildRun() noexcept;

    std::wstring text_;
    std::unique_ptr<graphics::TextRun> run_;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_CONTENT_PRESENTER_H_
