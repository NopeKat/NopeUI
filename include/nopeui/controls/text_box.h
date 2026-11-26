#ifndef NOPEUI_CONTROLS_TEXT_BOX_H_
#define NOPEUI_CONTROLS_TEXT_BOX_H_

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "nopeui/base/ref.h"
#include "nopeui/graphics/text.h"
#include "nopeui/layout/element.h"

namespace nopeui {

// Single-line editable text field with caret, submit handling and a placeholder.
// Selection, clipboard and IME composition are not implemented yet.
class TextBox : public Element {
public:
    static Ref<TextBox> Create(
        std::wstring text = {},
        graphics::TextStyle style = {}) noexcept;

    const std::wstring& Text() const noexcept {
        return text_;
    }
    void SetText(std::wstring text) noexcept;

    void SetPlaceholder(std::wstring placeholder) noexcept;

    void SetOnSubmit(std::function<void()> handler) {
        submit_ = std::move(handler);
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;
    void OnPointerEnter() noexcept override;
    void OnPointerLeave() noexcept override;
    void OnPointerDown(Point point) noexcept override;
    bool IsFocusable() const noexcept override;
    bool OnKeyDown(unsigned int key) noexcept override;
    bool OnChar(wchar_t character) noexcept override;
    void OnPointerMove(Point point) noexcept override;
    void OnPointerUp(Point point) noexcept override;
    void OnImeStart() noexcept override;
    void OnImeComposition(std::wstring_view text) noexcept override;
    void OnImeResult(std::wstring_view text) noexcept override;
    void OnImeEnd() noexcept override;
    Point ImeCaretPoint() const noexcept override;

private:
    TextBox() noexcept = default;

    void RebuildRun() noexcept;
    void RebuildPlaceholder() noexcept;
    void RebuildCompositionRun() noexcept;
    void ClearComposition() noexcept;
    void DropSelection() noexcept;
    void MoveCaret(std::size_t position, bool extend) noexcept;
    void EnsureCaretVisible() noexcept;
    std::size_t SelectionStart() const noexcept;
    std::size_t SelectionEnd() const noexcept;
    bool HasSelection() const noexcept;

    std::wstring text_;
    std::wstring placeholder_;
    std::wstring composition_;
    graphics::TextStyle style_;
    std::unique_ptr<graphics::TextRun> run_;
    std::unique_ptr<graphics::TextRun> placeholder_run_;
    std::unique_ptr<graphics::TextRun> composition_run_;
    std::function<void()> submit_;
    std::size_t caret_ = 0;
    std::size_t anchor_ = 0;
    float scroll_x_ = 0.0f;
    bool hovered_ = false;
    bool dragging_ = false;
    bool composing_ = false;
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_TEXT_BOX_H_
