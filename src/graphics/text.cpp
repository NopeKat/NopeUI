#include "nopeui/graphics/text.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <cstdio>
#include <string>

namespace nopeui::graphics {
namespace {

using Microsoft::WRL::ComPtr;

std::string HResultMessage(const char* operation, HRESULT result) {
    char buffer[160];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%s failed with HRESULT 0x%08X",
        operation,
        static_cast<unsigned int>(result));
    return std::string(buffer);
}

Result<IDWriteFactory*> TextFactory() {
    static ComPtr<IDWriteFactory> factory;
    if (factory != nullptr) {
        return Result<IDWriteFactory*>::Ok(factory.Get());
    }
    HRESULT result = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
    if (FAILED(result)) {
        return Result<IDWriteFactory*>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("DWriteCreateFactory", result)});
    }
    return Result<IDWriteFactory*>::Ok(factory.Get());
}

}  // namespace

struct TextRun::Impl {
    ComPtr<IDWriteFactory> factory;
    ComPtr<IDWriteTextFormat> format;
    ComPtr<IDWriteTextLayout> layout;
    Size size;
};

TextRun::TextRun() noexcept = default;

TextRun::~TextRun() = default;

Result<std::unique_ptr<TextRun>> TextRun::Create(
    std::wstring_view text,
    const TextStyle& style,
    float max_width) noexcept {
    if (text.empty()) {
        return Result<std::unique_ptr<TextRun>>::Fail(
            Error{ErrorCode::InvalidArgument, "text is empty"});
    }
    if (style.size <= 0.0f) {
        return Result<std::unique_ptr<TextRun>>::Fail(
            Error{ErrorCode::InvalidArgument, "text size must be positive"});
    }

    auto factory_result = TextFactory();
    if (!factory_result.ok()) {
        return Result<std::unique_ptr<TextRun>>::Fail(factory_result.error());
    }

    auto impl = std::make_unique<Impl>();
    impl->factory = factory_result.value();

    HRESULT result = impl->factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        style.bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        style.size,
        L"en-US",
        impl->format.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<TextRun>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateTextFormat", result)});
    }

    const float layout_width = max_width > 1.0f ? max_width : 1.0f;
    result = impl->factory->CreateTextLayout(
        text.data(),
        static_cast<UINT32>(text.size()),
        impl->format.Get(),
        layout_width,
        8192.0f,
        impl->layout.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<TextRun>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateTextLayout", result)});
    }

    impl->layout->SetWordWrapping(
        style.wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (style.ellipsis) {
        ComPtr<IDWriteInlineObject> ellipsis;
        if (SUCCEEDED(impl->factory->CreateEllipsisTrimmingSign(
                impl->format.Get(), ellipsis.GetAddressOf()))) {
            DWRITE_TRIMMING trimming{};
            trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
            impl->layout->SetTrimming(&trimming, ellipsis.Get());
        }
    }

    DWRITE_TEXT_METRICS metrics{};
    result = impl->layout->GetMetrics(&metrics);
    if (FAILED(result)) {
        return Result<std::unique_ptr<TextRun>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("GetMetrics", result)});
    }
    impl->size.width = metrics.widthIncludingTrailingWhitespace;
    impl->size.height = metrics.height;

    std::unique_ptr<TextRun> run(new TextRun());
    run->impl_ = std::move(impl);
    return Result<std::unique_ptr<TextRun>>::Ok(std::move(run));
}

Size TextRun::Measure() const noexcept {
    return impl_->size;
}

void TextRun::Draw(
    ID2D1RenderTarget* target,
    Point origin,
    std::uint32_t color) const noexcept {
    if (target == nullptr || impl_ == nullptr) {
        return;
    }
    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(D2D1::ColorF(color), brush.GetAddressOf()))) {
        return;
    }
    target->DrawTextLayout(D2D1::Point2F(origin.x, origin.y), impl_->layout.Get(), brush.Get());
}

}  // namespace nopeui::graphics
