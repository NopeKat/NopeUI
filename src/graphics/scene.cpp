#include "nopeui/graphics/scene.h"

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

ScenePalette LightPalette() noexcept {
    return ScenePalette{};
}

ScenePalette DarkPalette() noexcept {
    ScenePalette palette;
    palette.background = 0x202020;
    palette.accent = 0x60CDFF;
    palette.text = 0xFFFFFF;
    return palette;
}

Result<void> PaintPlaceholderScene(
    ID2D1RenderTarget* target,
    const ScenePalette& palette,
    float width,
    float height) noexcept {
    if (target == nullptr || width <= 0.0f || height <= 0.0f) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidArgument, "scene target or size is invalid"});
    }

    ComPtr<ID2D1SolidColorBrush> background_brush;
    ComPtr<ID2D1SolidColorBrush> accent_brush;
    ComPtr<ID2D1SolidColorBrush> text_brush;
    HRESULT result = target->CreateSolidColorBrush(
        D2D1::ColorF(palette.background), background_brush.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateSolidColorBrush", result)});
    }
    result = target->CreateSolidColorBrush(
        D2D1::ColorF(palette.accent), accent_brush.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateSolidColorBrush", result)});
    }
    result = target->CreateSolidColorBrush(
        D2D1::ColorF(palette.text), text_brush.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateSolidColorBrush", result)});
    }

    target->Clear(D2D1::ColorF(palette.background));
    target->FillRectangle(D2D1::RectF(0.0f, 0.0f, width, 4.0f), accent_brush.Get());

    auto factory_result = TextFactory();
    if (!factory_result.ok()) {
        return Result<void>::Fail(factory_result.error());
    }
    IDWriteFactory* factory = factory_result.value();

    ComPtr<IDWriteTextFormat> format;
    result = factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        26.0f,
        L"en-US",
        format.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateTextFormat", result)});
    }

    ComPtr<IDWriteTextLayout> layout;
    result = factory->CreateTextLayout(
        L"NopeUI",
        6,
        format.Get(),
        800.0f,
        60.0f,
        layout.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateTextLayout", result)});
    }

    target->DrawTextLayout(D2D1::Point2F(24.0f, 28.0f), layout.Get(), text_brush.Get());
    return Result<void>::Ok();
}

}  // namespace nopeui::graphics
