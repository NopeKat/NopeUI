#include "nopeui/controls/content_presenter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include "nopeui/graphics/text.h"
#include "nopeui/theme/theme.h"

namespace nopeui {

namespace {

const PropertyMetadata kContentPresenterProperties[] = {
    {L"Text", PropertyValueType::String, PropertyFlags::None, PropertyValue(std::wstring())},
};

}  // namespace

Ref<ContentPresenter> ContentPresenter::Create() noexcept {
    return Ref<ContentPresenter>(new ContentPresenter());
}

void ContentPresenter::SetText(std::wstring text) noexcept {
    if (text_ == text) {
        return;
    }
    text_ = std::move(text);
    RebuildRun();
    InvalidateLayout();
}

void ContentPresenter::RebuildRun() noexcept {
    graphics::TextStyle style;
    style.size = 14.0f;
    auto created = graphics::TextRun::Create(text_, style, 8192.0f);
    if (created.ok()) {
        run_ = std::move(created.value());
    } else {
        run_.reset();
    }
}

Size ContentPresenter::MeasureOverride(Size available) noexcept {
    const Size measured = run_ != nullptr
        ? run_->Measure()
        : Size{static_cast<float>(text_.size()) * 8.0f, 20.0f};
    return Size{
        std::min(measured.width, available.width),
        std::min(measured.height, available.height)};
}

void ContentPresenter::PaintContent(ID2D1RenderTarget* target) noexcept {
    if (run_ == nullptr) {
        return;
    }
    const std::uint32_t inherited = Foreground();
    const std::uint32_t color = inherited != 0 ? inherited : Colors().text;
    run_->Draw(target, Point{bounds_.x, bounds_.y}, color);
}

const PropertyMetadata* ContentPresenter::ControlPropertyTable(
    std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kContentPresenterProperties);
    }
    return kContentPresenterProperties;
}

bool ContentPresenter::GetControlProperty(
    std::wstring_view name,
    PropertyValue* value) const noexcept {
    if (name == L"Text") {
        *value = PropertyValue(text_);
        return true;
    }
    return false;
}

bool ContentPresenter::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    if (name != L"Text") {
        return false;
    }
    std::wstring text;
    if (!value.AsString(&text)) {
        return false;
    }
    SetText(std::move(text));
    return true;
}

}  // namespace nopeui
