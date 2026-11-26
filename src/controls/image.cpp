#include "nopeui/controls/image.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>
#include <dxgiformat.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

#include "nopeui/theme/theme.h"

namespace nopeui {

using Microsoft::WRL::ComPtr;

namespace {

const PropertyMetadata kImageProperties[] = {
    {L"Stretch", PropertyValueType::Int, PropertyFlags::AffectsRender,
     PropertyValue(static_cast<int>(ImageStretch::Uniform))},
    {L"NaturalWidth", PropertyValueType::Int, PropertyFlags::None, PropertyValue(0)},
    {L"NaturalHeight", PropertyValueType::Int, PropertyFlags::None, PropertyValue(0)},
};

void Premultiply(
    const graphics::Image& image,
    std::vector<std::uint32_t>& out) {
    out.resize(image.pixels.size());
    for (std::size_t index = 0; index < image.pixels.size(); ++index) {
        const std::uint32_t pixel = image.pixels[index];
        const std::uint32_t alpha = pixel >> 24;
        if (alpha == 0xff) {
            out[index] = pixel;
            continue;
        }
        const std::uint32_t blue = ((pixel & 0x000000ffu) * alpha + 127u) / 255u;
        const std::uint32_t green = (((pixel >> 8) & 0x000000ffu) * alpha + 127u) / 255u;
        const std::uint32_t red = (((pixel >> 16) & 0x000000ffu) * alpha + 127u) / 255u;
        out[index] = (alpha << 24) | (red << 16) | (green << 8) | blue;
    }
}

}  // namespace

struct Image::BitmapCache {
    ID2D1RenderTarget* target = nullptr;
    ComPtr<ID2D1Bitmap> bitmap;
};

Ref<Image> Image::Create() noexcept {
    return Ref<Image>(new Image());
}

Result<Ref<Image>> Image::CreateFromFile(std::string_view utf8_path) noexcept {
    auto loaded = graphics::LoadPng(utf8_path);
    if (!loaded.ok()) {
        return Result<Ref<Image>>::Fail(loaded.error());
    }
    Ref<Image> control(new Image());
    control->SetImage(std::move(loaded.value()));
    return Result<Ref<Image>>::Ok(std::move(control));
}

Image::~Image() = default;

void Image::SetImage(graphics::Image image) noexcept {
    image_ = std::move(image);
    cache_.reset();
    InvalidateLayout();
}

bool Image::LoadFromFile(std::string_view utf8_path) noexcept {
    auto loaded = graphics::LoadPng(utf8_path);
    if (!loaded.ok()) {
        return false;
    }
    SetImage(std::move(loaded.value()));
    return true;
}

void Image::SetStretch(ImageStretch stretch) noexcept {
    if (stretch_ == stretch) {
        return;
    }
    stretch_ = stretch;
    Invalidate();
}

void Image::SetDisplaySize(Size size) noexcept {
    if (display_size_ == size) {
        return;
    }
    display_size_ = size;
    InvalidateLayout();
}

Size Image::MeasureOverride(Size available) noexcept {
    if (image_.Empty()) {
        const float width = std::min(160.0f, available.width > 0.0f ? available.width : 160.0f);
        const float height = std::min(90.0f, available.height > 0.0f ? available.height : 90.0f);
        return Size{width, height};
    }
    const float natural_width = static_cast<float>(image_.width);
    const float natural_height = static_cast<float>(image_.height);
    const float width = display_size_.width > 0.0f ? display_size_.width : natural_width;
    const float height = display_size_.height > 0.0f ? display_size_.height : natural_height;
    return Size{
        available.width > 0.0f ? std::min(width, available.width) : width,
        available.height > 0.0f ? std::min(height, available.height) : height};
}

void Image::PaintContent(ID2D1RenderTarget* target) noexcept {
    const Theme& colors = Colors();
    if (image_.Empty()) {
        ComPtr<ID2D1SolidColorBrush> fill;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(colors.button), fill.GetAddressOf()))) {
            target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom()),
                    4.0f,
                    4.0f),
                fill.Get());
        }
        ComPtr<ID2D1SolidColorBrush> line;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                D2D1::ColorF(colors.surface_border), line.GetAddressOf()))) {
            target->DrawLine(
                D2D1::Point2F(bounds_.x + 8.0f, bounds_.y + 8.0f),
                D2D1::Point2F(bounds_.Right() - 8.0f, bounds_.Bottom() - 8.0f),
                line.Get(),
                1.0f);
            target->DrawLine(
                D2D1::Point2F(bounds_.Right() - 8.0f, bounds_.y + 8.0f),
                D2D1::Point2F(bounds_.x + 8.0f, bounds_.Bottom() - 8.0f),
                line.Get(),
                1.0f);
        }
        return;
    }

    if (!cache_ || cache_->target != target) {
        cache_ = std::make_unique<BitmapCache>();
        cache_->target = target;
        std::vector<std::uint32_t> premultiplied;
        Premultiply(image_, premultiplied);
        const D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        const D2D1_SIZE_U size = D2D1::SizeU(
            static_cast<UINT32>(image_.width),
            static_cast<UINT32>(image_.height));
        target->CreateBitmap(
            size,
            premultiplied.data(),
            static_cast<UINT32>(image_.width) * sizeof(std::uint32_t),
            properties,
            cache_->bitmap.GetAddressOf());
    }
    if (cache_->bitmap == nullptr) {
        return;
    }

    const float image_width = static_cast<float>(image_.width);
    const float image_height = static_cast<float>(image_.height);
    D2D1_RECT_F destination = D2D1::RectF(
        bounds_.x, bounds_.y, bounds_.Right(), bounds_.Bottom());
    D2D1_RECT_F source = D2D1::RectF(0.0f, 0.0f, image_width, image_height);

    switch (stretch_) {
        case ImageStretch::None: {
            destination = D2D1::RectF(
                bounds_.x + (bounds_.width - image_width) / 2.0f,
                bounds_.y + (bounds_.height - image_height) / 2.0f,
                bounds_.x + (bounds_.width + image_width) / 2.0f,
                bounds_.y + (bounds_.height + image_height) / 2.0f);
            break;
        }
        case ImageStretch::Fill:
            break;
        case ImageStretch::Uniform: {
            const float scale = std::min(
                bounds_.width / image_width,
                bounds_.height / image_height);
            const float width = image_width * scale;
            const float height = image_height * scale;
            destination = D2D1::RectF(
                bounds_.x + (bounds_.width - width) / 2.0f,
                bounds_.y + (bounds_.height - height) / 2.0f,
                bounds_.x + (bounds_.width + width) / 2.0f,
                bounds_.y + (bounds_.height + height) / 2.0f);
            break;
        }
        case ImageStretch::UniformToFill: {
            const float scale = std::max(
                bounds_.width / image_width,
                bounds_.height / image_height);
            const float source_width = bounds_.width / scale;
            const float source_height = bounds_.height / scale;
            const float source_x = (image_width - source_width) / 2.0f;
            const float source_y = (image_height - source_height) / 2.0f;
            source = D2D1::RectF(
                source_x,
                source_y,
                source_x + source_width,
                source_y + source_height);
            break;
        }
    }

    PushContentClip(target);
    target->DrawBitmap(
        cache_->bitmap.Get(),
        destination,
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        &source);
    PopContentClip(target);
}

const PropertyMetadata* Image::ControlPropertyTable(std::size_t* count) const noexcept {
    if (count != nullptr) {
        *count = std::size(kImageProperties);
    }
    return kImageProperties;
}

bool Image::GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept {
    if (name == L"Stretch") {
        *value = PropertyValue(static_cast<int>(stretch_));
        return true;
    }
    if (name == L"NaturalWidth") {
        *value = PropertyValue(image_.width);
        return true;
    }
    if (name == L"NaturalHeight") {
        *value = PropertyValue(image_.height);
        return true;
    }
    return false;
}

bool Image::SetControlProperty(
    std::wstring_view name,
    const PropertyValue& value) noexcept {
    if (name != L"Stretch") {
        return false;
    }
    int stretch = 0;
    if (!value.AsInt(&stretch) ||
        stretch < static_cast<int>(ImageStretch::None) ||
        stretch > static_cast<int>(ImageStretch::UniformToFill)) {
        return false;
    }
    SetStretch(static_cast<ImageStretch>(stretch));
    return true;
}

}  // namespace nopeui
