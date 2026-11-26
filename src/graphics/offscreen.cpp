#include "nopeui/graphics/offscreen.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstring>
#include <memory>
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

struct OffscreenSurface::Impl {
    int width = 0;
    int height = 0;
    ComPtr<ID2D1Factory> d2d_factory;
    ComPtr<IWICImagingFactory> wic_factory;
    ComPtr<IWICBitmap> bitmap;
    ComPtr<ID2D1RenderTarget> target;
};

OffscreenSurface::OffscreenSurface() noexcept = default;

OffscreenSurface::~OffscreenSurface() = default;

Result<std::unique_ptr<OffscreenSurface>> OffscreenSurface::Create(
    int width,
    int height,
    float dpi) noexcept {
    if (width <= 0 || height <= 0 || dpi <= 0.0f) {
        return Result<std::unique_ptr<OffscreenSurface>>::Fail(
            Error{ErrorCode::InvalidArgument, "offscreen size and dpi must be positive"});
    }

    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(result) && result != RPC_E_CHANGED_MODE) {
        return Result<std::unique_ptr<OffscreenSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CoInitializeEx", result)});
    }

    auto impl = std::make_unique<Impl>();

    result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(impl->wic_factory.GetAddressOf()));
    if (FAILED(result)) {
        return Result<std::unique_ptr<OffscreenSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CoCreateInstance(WIC)", result)});
    }

    D2D1_FACTORY_OPTIONS factory_options{};
    result = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory),
        &factory_options,
        reinterpret_cast<void**>(impl->d2d_factory.GetAddressOf()));
    if (FAILED(result)) {
        return Result<std::unique_ptr<OffscreenSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("D2D1CreateFactory", result)});
    }

    result = impl->wic_factory->CreateBitmap(
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        impl->bitmap.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<OffscreenSurface>>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("WIC::CreateBitmap", result)});
    }

    const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi,
        dpi);
    result = impl->d2d_factory->CreateWicBitmapRenderTarget(
        impl->bitmap.Get(),
        properties,
        impl->target.GetAddressOf());
    if (FAILED(result)) {
        return Result<std::unique_ptr<OffscreenSurface>>::Fail(
            Error{ErrorCode::PlatformFailure,
                  HResultMessage("CreateWicBitmapRenderTarget", result)});
    }

    // Golden images must not depend on the host display configuration.
    impl->target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    impl->target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    impl->width = width;
    impl->height = height;

    std::unique_ptr<OffscreenSurface> surface(new OffscreenSurface());
    surface->impl_ = std::move(impl);
    return Result<std::unique_ptr<OffscreenSurface>>::Ok(std::move(surface));
}

ID2D1RenderTarget* OffscreenSurface::Target() const noexcept {
    return impl_->target.Get();
}

int OffscreenSurface::Width() const noexcept {
    return impl_->width;
}

int OffscreenSurface::Height() const noexcept {
    return impl_->height;
}

Result<void> OffscreenSurface::BeginDraw() noexcept {
    if (impl_ == nullptr || impl_->target == nullptr) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidState, "offscreen surface is not initialized"});
    }
    impl_->target->BeginDraw();
    return Result<void>::Ok();
}

Result<void> OffscreenSurface::EndDraw() noexcept {
    if (impl_ == nullptr || impl_->target == nullptr) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidState, "offscreen surface is not initialized"});
    }
    const HRESULT result = impl_->target->EndDraw();
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("EndDraw", result)});
    }
    return Result<void>::Ok();
}

Result<void> OffscreenSurface::SavePng(std::string_view utf8_path) const noexcept {
    if (impl_ == nullptr || impl_->bitmap == nullptr) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidState, "offscreen surface is not initialized"});
    }

    const int width = Width();
    const int height = Height();
    WICRect rect{0, 0, width, height};
    IWICBitmapLock* raw_lock = nullptr;
    HRESULT result = impl_->bitmap->Lock(&rect, WICBitmapLockRead, &raw_lock);
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("WIC::Lock", result)});
    }
    ComPtr<IWICBitmapLock> lock;
    lock.Attach(raw_lock);

    UINT stride = 0;
    UINT buffer_size = 0;
    BYTE* data = nullptr;
    result = lock->GetStride(&stride);
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("GetStride", result)});
    }
    result = lock->GetDataPointer(&buffer_size, &data);
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("GetDataPointer", result)});
    }

    std::vector<std::uint32_t> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    const std::size_t row_bytes = static_cast<std::size_t>(width) * sizeof(std::uint32_t);
    if (stride == row_bytes) {
        std::memcpy(pixels.data(), data, row_bytes * static_cast<std::size_t>(height));
    } else {
        for (int row = 0; row < height; ++row) {
            std::memcpy(
                pixels.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width),
                data + static_cast<std::size_t>(row) * stride,
                row_bytes);
        }
    }
    return ::nopeui::graphics::SavePng(utf8_path, width, height, pixels);
}

}  // namespace nopeui::graphics
