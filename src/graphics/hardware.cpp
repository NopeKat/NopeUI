#include "nopeui/graphics/hardware.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "nopeui/graphics/image.h"

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

}  // namespace

struct HardwareSurface::Impl {
    int width = 0;
    int height = 0;
    bool warp = false;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISurface> dxgi_surface;
    ComPtr<ID2D1Factory1> d2d_factory;
    ComPtr<ID2D1Device> d2d_device;
    ComPtr<ID2D1DeviceContext> d2d_context;
    ComPtr<ID3D11Texture2D> target_texture;
    ComPtr<ID2D1Bitmap1> target_bitmap;
};

HardwareSurface::HardwareSurface() noexcept = default;

HardwareSurface::~HardwareSurface() = default;

Result<std::unique_ptr<HardwareSurface>> HardwareSurface::Create(
    int width,
    int height,
    float dpi,
    bool allow_warp) noexcept {
    if (width <= 0 || height <= 0 || dpi <= 0.0f) {
        return Result<std::unique_ptr<HardwareSurface>>::Fail(
            Error{ErrorCode::InvalidArgument, "hardware surface size and dpi must be positive"});
    }

    auto impl = std::make_unique<Impl>();
    impl->width = width;
    impl->height = height;

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        impl->device.GetAddressOf(),
        nullptr,
        impl->context.GetAddressOf());
    if (FAILED(result)) {
        if (!allow_warp) {
            return Result<std::unique_ptr<HardwareSurface>>::Fail(
                Error{ErrorCode::PlatformFailure, HResultMessage("D3D11CreateDevice", result)});
        }
        result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            impl->device.GetAddressOf(),
            nullptr,
            impl->context.GetAddressOf());
        if (FAILED(result)) {
            return Result<std::unique_ptr<HardwareSurface>>::Fail(
                Error{ErrorCode::PlatformFailure, HResultMessage("D3D11CreateDevice(WARP)", result)});
        }
        impl->warp = true;
    }

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = static_cast<UINT>(width);
    texture_desc.Height = static_cast<UINT>(height);
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    result = impl->device->CreateTexture2D(
        &texture_desc,
        nullptr,
        impl->target_texture.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<HardwareSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateTexture2D", result)});
    }

    result = impl->target_texture.As(&impl->dxgi_surface);
    if (FAILED(result)) {
        return Result<std::unique_ptr<HardwareSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("QueryInterface IDXGISurface", result)});
    }

    D2D1_FACTORY_OPTIONS factory_options{};
    result = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &factory_options,
        reinterpret_cast<void**>(impl->d2d_factory.GetAddressOf()));
    if (FAILED(result)) {
        return Result<std::unique_ptr<HardwareSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("D2D1CreateFactory", result)});
    }

    ComPtr<IDXGIDevice> dxgi_device;
    result = impl->device.As(&dxgi_device);
    if (FAILED(result)) {
        return Result<std::unique_ptr<HardwareSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("QueryInterface IDXGIDevice", result)});
    }

    result = impl->d2d_factory->CreateDevice(dxgi_device.Get(), impl->d2d_device.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<HardwareSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("D2D::CreateDevice", result)});
    }

    result = impl->d2d_device->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        impl->d2d_context.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<HardwareSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateDeviceContext", result)});
    }

    const D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi,
        dpi);
    result = impl->d2d_context->CreateBitmapFromDxgiSurface(
        impl->dxgi_surface.Get(),
        &bitmap_properties,
        impl->target_bitmap.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<HardwareSurface>>::Fail(
            Error{ErrorCode::PlatformFailure,
                  HResultMessage("CreateBitmapFromDxgiSurface", result)});
    }

    impl->d2d_context->SetTarget(impl->target_bitmap.Get());
    impl->d2d_context->SetDpi(dpi, dpi);
    impl->d2d_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    impl->d2d_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    std::unique_ptr<HardwareSurface> surface(new HardwareSurface());
    surface->impl_ = std::move(impl);
    return Result<std::unique_ptr<HardwareSurface>>::Ok(std::move(surface));
}

ID2D1RenderTarget* HardwareSurface::Target() const noexcept {
    return impl_->d2d_context.Get();
}

int HardwareSurface::Width() const noexcept {
    return impl_->width;
}

int HardwareSurface::Height() const noexcept {
    return impl_->height;
}

bool HardwareSurface::IsWarp() const noexcept {
    return impl_->warp;
}

Result<void> HardwareSurface::BeginDraw() noexcept {
    if (impl_ == nullptr || impl_->d2d_context == nullptr) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidState, "hardware surface is not initialized"});
    }
    impl_->d2d_context->BeginDraw();
    return Result<void>::Ok();
}

Result<void> HardwareSurface::EndDraw() noexcept {
    if (impl_ == nullptr || impl_->d2d_context == nullptr) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidState, "hardware surface is not initialized"});
    }
    const HRESULT result = impl_->d2d_context->EndDraw();
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("EndDraw", result)});
    }
    return Result<void>::Ok();
}

Result<void> HardwareSurface::SavePng(std::string_view utf8_path) noexcept {
    if (impl_ == nullptr || impl_->device == nullptr) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidState, "hardware surface is not initialized"});
    }

    D3D11_TEXTURE2D_DESC staging_desc{};
    staging_desc.Width = static_cast<UINT>(impl_->width);
    staging_desc.Height = static_cast<UINT>(impl_->height);
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    staging_desc.SampleDesc.Count = 1;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> staging;
    HRESULT result = impl_->device->CreateTexture2D(
        &staging_desc,
        nullptr,
        staging.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateTexture2D(staging)", result)});
    }

    impl_->context->CopyResource(staging.Get(), impl_->target_texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    result = impl_->context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("Map", result)});
    }

    std::vector<std::uint32_t> pixels(
        static_cast<std::size_t>(impl_->width) * static_cast<std::size_t>(impl_->height));
    const std::size_t row_bytes =
        static_cast<std::size_t>(impl_->width) * sizeof(std::uint32_t);
    for (int row = 0; row < impl_->height; ++row) {
        std::memcpy(
            pixels.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(impl_->width),
            static_cast<const std::uint8_t*>(mapped.pData) +
                static_cast<std::size_t>(row) * mapped.RowPitch,
            row_bytes);
    }
    impl_->context->Unmap(staging.Get(), 0);

    return ::nopeui::graphics::SavePng(utf8_path, impl_->width, impl_->height, pixels);
}

}  // namespace nopeui::graphics
