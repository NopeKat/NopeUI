#include "nopeui/controls/button.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>
#include <wrl/client.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include "nopeui/controls/content_presenter.h"
#include "nopeui/controls/control_state.h"
#include "nopeui/graphics/text.h"
#include "nopeui/theme/control_template.h"
#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

constexpr float kPaddingX = 16.0f;
constexpr float kPaddingY = 8.0f;
constexpr float kMinWidth = 88.0f;
constexpr float kMinHeight = 32.0f;

const PropertyMetadata kButtonProperties[] = {
    {L"Text", PropertyValueType::String, PropertyFlags::None, PropertyValue(std::wstring())},
    {L"CornerRadius", PropertyValueType::Float, PropertyFlags::AffectsRender, PropertyValue(4.0f)},
};

// CommonStates names, matching the WPF vocabulary so templates read the same.
std::wstring StateName(ControlState state) {
    switch (state) {
        case ControlState::Pressed:
            return L"Pressed";
        case ControlState::Hovered:
            return L"PointerOver";
        case ControlState::Disabled:
            return L"Disabled";
        case ControlState::Normal:
            break;
    }
    return L"Normal";
}

void FillContentPresenters(Element& element, std::wstring_view text) {    if (auto* presenter = dynamic_cast<ContentPresenter*>(&element)) {
        if (presenter->Text().empty()) {
            presenter->SetText(std::wstring(text));
        }
    }
    std::vector<Element*> children;
    element.CollectChildren(children);
    for (Element* child : children) {
        if (child != nullptr) {
            FillContentPresenters(*child, text);
        }
    }
}

}  // namespace

Ref<Button> Button::Create(std::wstring text) noexcept {
    Ref<Button> button(new Button());
    button->text_ = std::move(text);
    graphics::TextStyle style;
    style.size = 14.0f;
    auto run = graphics::TextRun::Create(button->text_, style, 8192.0f);
    if (run.ok()) {
        button->run_ = std::move(run.value());
    }
    return button;
}

Size Button::MeasureOverride(Size available) noexcept {
    if (template_root_) {
        template_root_->Measure(available);
        const Size desired = template_root_->DesiredSize();
        return Size{
            std::min(std::max(desired.width, kMinWidth), available.width),
            std::min(std::max(desired.height, kMinHeight), available.height)};
    }
    const Size text = run_ != nullptr
        ? run_->Measure()
        : Size{static_cast<float>(text_.size()) * 8.0f, 20.0f};
    return Size{
        std::min(std::max(text.width + kPaddingX * 2.0f, kMinWidth), available.width),
        std::min(std::max(text.height + kPaddingY * 2.0f, kMinHeight), available.height)};
}

Size Button::ArrangeOverride(Size final_size) noexcept {
    if (template_root_) {
        template_root_->Arrange(
            Rect{bounds_.x, bounds_.y, final_size.width, final_size.height});
    }
    return final_size;
}

void Button::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    if (template_root_) {
        template_root_->Paint(target);
        if (IsFocused() && IsEnabled()) {
            ComPtr<ID2D1SolidColorBrush> focus;
            if (SUCCEEDED(target->CreateSolidColorBrush(
                    D2D1::ColorF(colors.accent), focus.GetAddressOf()))) {
                target->DrawRectangle(
                    D2D1::RectF(
                        bounds_.x + 2.5f,
                        bounds_.y + 2.5f,
                        bounds_.Right() - 2.5f,
                        bounds_.Bottom() - 2.5f),
                    focus.Get(),
                    1.0f);
            }
        }
        return;
    }
    const ControlState state = ResolveControlState(IsEnabled(), hovered_, pressed_);
    const std::wstring state_name = StateName(state);
    TemplatePart background_part{};
    TemplatePart content_part{};
    if (control_template_) {
        background_part =
            control_template_->ResolvePart(L"Background", L"CommonStates", state_name);
        content_part =
            control_template_->ResolvePart(L"Content", L"CommonStates", state_name);
    }

    // The template wins when it set a color; otherwise the theme state decides.
    std::uint32_t background = colors.button;
    switch (state) {
        case ControlState::Disabled:
            background = colors.button_disabled;
            break;
        case ControlState::Pressed:
            background = colors.button_pressed;
            break;
        case ControlState::Hovered:
            background = colors.button_hover;
            break;
        case ControlState::Normal:
            break;
    }
    if (background_part.background != 0) {
        background = background_part.background;
    }
    const std::uint32_t outline_color = background_part.border != 0
        ? background_part.border
        : (state == ControlState::Disabled
            ? colors.button_disabled_border
            : colors.button_border);
    const std::uint32_t text_color = content_part.foreground != 0
        ? content_part.foreground
        : (state == ControlState::Disabled ? colors.text_disabled : colors.text);

    const float radius = background_part.corner_radius > 0.0f
        ? background_part.corner_radius
        : corner_radius_;
    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(
        D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
        radius,
        radius);

    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(
            D2D1::ColorF(background), brush.GetAddressOf()))) {
        return;
    }
    target->FillRoundedRectangle(rounded, brush.Get());

    ComPtr<ID2D1SolidColorBrush> outline;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            D2D1::ColorF(outline_color), outline.GetAddressOf()))) {
        const D2D1_ROUNDED_RECT border = D2D1::RoundedRect(
            D2D1::RectF(
                bounds_.x + 0.5f,
                bounds_.y + 0.5f,
                bounds_.Right() - 0.5f,
                bounds_.Bottom() - 0.5f),
            radius,
            radius);
        target->DrawRoundedRectangle(border, outline.Get(), 1.0f);
    }

    if (run_ == nullptr) {
        return;
    }
    const Size text = run_->Measure();
    run_->Draw(
        target,
        Point{
            bounds_.x + (bounds_.width - text.width) / 2.0f,
            bounds_.y + (bounds_.height - text.height) / 2.0f},
        text_color);

    if (IsFocused() && IsEnabled()) {
        ComPtr<ID2D1SolidColorBrush> focus;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(colors.accent), focus.GetAddressOf()))) {
            target->DrawRectangle(
                D2D1::RectF(
                    bounds_.x + 2.5f,
                    bounds_.y + 2.5f,
                    bounds_.Right() - 2.5f,
                    bounds_.Bottom() - 2.5f),
                focus.Get(),
                1.0f);
        }
    }
}

void Button::OnPointerEnter() noexcept {
    hovered_ = true;
}

void Button::OnPointerLeave() noexcept {
    hovered_ = false;
    pressed_ = false;
}

void Button::OnPointerDown(Point point) noexcept {
    if (Contains(point)) {
        pressed_ = true;
        hovered_ = true;
        Invalidate();
    }
}

void Button::OnPointerUp(Point point) noexcept {
    const bool clicked = pressed_ && Contains(point);
    pressed_ = false;
    Invalidate();
    if (clicked) {
        click_.Raise();
    }
}

bool Button::IsFocusable() const noexcept {
    return IsEnabled();
}

bool Button::OnKeyDown(unsigned int key) noexcept {
    if (!IsEnabled() || (key != VK_SPACE && key != VK_RETURN)) {
        return false;
    }
    if (!pressed_) {
        pressed_ = true;
        Invalidate();
    }
    return true;
}

bool Button::OnKeyUp(unsigned int key) noexcept {
    if (!IsEnabled() || (key != VK_SPACE && key != VK_RETURN)) {
        return false;
    }
    const bool clicked = pressed_;
    pressed_ = false;
    Invalidate();
    if (clicked) {
        click_.Raise();
    }
    return true;
}

void Button::ApplyControlTemplate(const ControlTemplate& control_template) noexcept {
    template_bindings_.clear();
    template_root_.Reset();
    if (control_template_ && control_template_.Get() != &control_template) {
        control_template_.Reset();
    }
    if (!control_template_) {
        const_cast<ControlTemplate&>(control_template).AddRef();
        control_template_ = Ref<ControlTemplate>(const_cast<ControlTemplate*>(&control_template));
    }

    if (control_template.HasFactory()) {
        TemplateInstance instance = control_template.Instantiate();
        if (instance.root) {
            template_root_ = std::move(instance.root);
            template_root_->SetParent(this);
            for (const TemplateBinding& binding : instance.bindings) {
                if (!binding.target) {
                    continue;
                }
                PropertyValue value;
                if (GetProperty(binding.source_property, &value)) {
                    binding.target->SetProperty(binding.target_property, value);
                }
                Element* target = binding.target.Get();
                const std::wstring source_property = binding.source_property;
                const std::wstring target_property = binding.target_property;
                template_bindings_.push_back(SubscribePropertyChanged(
                    [target, source_property, target_property](
                        const PropertyMetadata& metadata,
                        const PropertyValue&,
                        const PropertyValue& next) {
                        if (std::wstring_view(metadata.name) == source_property) {
                            target->SetProperty(target_property, next);
                        }
                    }));
            }
            FillContentPresenters(*template_root_, text_);
            InvalidateLayout();
            return;
        }
    }

    if (const TemplatePart* part = control_template.Find(L"Background")) {
        if (part->corner_radius > 0.0f) {
            corner_radius_ = part->corner_radius;
        }
    }
    InvalidateLayout();
}

const PropertyMetadata* Button::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kButtonProperties);
    }
    return kButtonProperties;
}

bool Button::GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept {
    if (name == L"Text") {
        *value = PropertyValue(text_);
        return true;
    }
    if (name == L"CornerRadius") {
        *value = PropertyValue(corner_radius_);
        return true;
    }
    return false;
}

bool Button::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    if (name != L"CornerRadius") {
        return false;
    }
    float radius = 0.0f;
    if (!value.AsFloat(&radius)) {
        return false;
    }
    corner_radius_ = std::max(0.0f, radius);
    Invalidate();
    return true;
}

AccessibleRole Button::AccessibilityRole() const noexcept {
    return AccessibleRole::Button;
}

std::wstring Button::AccessibilityName() const {
    return text_;
}

bool Button::HasAccessibilityInvoke() const noexcept {
    return true;
}

void Button::InvokeAccessibility() noexcept {
    if (IsEnabled()) {
        click_.Raise();
    }
}

}  // namespace nopeui
