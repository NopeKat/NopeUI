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

constexpr float kAccentHeight = 4.0f;
constexpr float kTextLeft = 24.0f;
constexpr float kTextTop = 28.0f;
constexpr float kTextWidth = 360.0f;
constexpr float kTextHeight = 64.0f;

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

bool AreaIntersects(
    float left,
    float top,
    float right,
    float bottom,
    float area_left,
    float area_top,
    float area_right,
    float area_bottom) noexcept {
    return right > area_left && left < area_right && bottom > area_top && top < area_bottom;
}

}  // namespace

struct ScenePainter::Impl {
    ID2D1RenderTarget* target = nullptr;
    ComPtr<ID2D1SolidColorBrush> background_brush;
    ComPtr<ID2D1SolidColorBrush> accent_brush;
    ComPtr<ID2D1SolidColorBrush> text_brush;
    ComPtr<IDWriteFactory> factory;
    ComPtr<IDWriteTextFormat> format;
    ComPtr<IDWriteTextLayout> layout;
};

ScenePainter::ScenePainter() noexcept = default;

ScenePainter::~ScenePainter() = default;

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

Result<std::unique_ptr<ScenePainter>> ScenePainter::Create(
    ID2D1RenderTarget* target,
    const ScenePalette& palette) noexcept {
    if (target == nullptr) {
        return Result<std::unique_ptr<ScenePainter>>::Fail(
            Error{ErrorCode::InvalidArgument, "scene target is null"});
    }

    auto impl = std::make_unique<Impl>();
    impl->target = target;

    HRESULT result = target->CreateSolidColorBrush(
        D2D1::ColorF(palette.background), impl->background_brush.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<ScenePainter>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateSolidColorBrush", result)});
    }
    result = target->CreateSolidColorBrush(
        D2D1::ColorF(palette.accent), impl->accent_brush.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<ScenePainter>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateSolidColorBrush", result)});
    }
    result = target->CreateSolidColorBrush(
        D2D1::ColorF(palette.text), impl->text_brush.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<ScenePainter>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateSolidColorBrush", result)});
    }

    auto factory_result = TextFactory();
    if (!factory_result.ok()) {
        return Result<std::unique_ptr<ScenePainter>>::Fail(factory_result.error());
    }
    impl->factory = factory_result.value();

    result = impl->factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        26.0f,
        L"en-US",
        impl->format.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<ScenePainter>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateTextFormat", result)});
    }

    result = impl->factory->CreateTextLayout(
        L"NopeUI",
        6,
        impl->format.Get(),
        kTextWidth,
        kTextHeight,
        impl->layout.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<ScenePainter>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateTextLayout", result)});
    }

    std::unique_ptr<ScenePainter> painter(new ScenePainter());
    painter->impl_ = std::move(impl);
    return Result<std::unique_ptr<ScenePainter>>::Ok(std::move(painter));
}

Result<void> ScenePainter::Paint(
    float width,
    float height,
    float left,
    float top,
    float right,
    float bottom) noexcept {
    if (impl_ == nullptr || impl_->target == nullptr) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidState, "scene painter is not initialized"});
    }
    if (right <= left || bottom <= top || width <= 0.0f || height <= 0.0f) {
        return Result<void>::Ok();
    }

    impl_->target->FillRectangle(
        D2D1::RectF(left, top, right, bottom),
        impl_->background_brush.Get());

    const bool draw_text = AreaIntersects(
        kTextLeft,
        kTextTop,
        kTextLeft + kTextWidth,
        kTextTop + kTextHeight,
        left,
        top,
        right,
        bottom);
    if (draw_text) {
        impl_->target->DrawTextLayout(
            D2D1::Point2F(kTextLeft, kTextTop),
            impl_->layout.Get(),
            impl_->text_brush.Get());
    }

    if (top < kAccentHeight) {
        impl_->target->FillRectangle(
            D2D1::RectF(0.0f, 0.0f, width, kAccentHeight),
            impl_->accent_brush.Get());
    }
    return Result<void>::Ok();
}

Result<void> PaintPlaceholderScene(
    ID2D1RenderTarget* target,
    const ScenePalette& palette,
    float width,
    float height) noexcept {
    auto painter_result = ScenePainter::Create(target, palette);
    if (!painter_result.ok()) {
        return Result<void>::Fail(painter_result.error());
    }
    auto& painter = painter_result.value();
    return painter->Paint(width, height, 0.0f, 0.0f, width, height);
}

}  // namespace nopeui::graphics
