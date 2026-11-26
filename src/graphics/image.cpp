#include "nopeui/graphics/image.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <wincodec.h>
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

Result<IWICImagingFactory*> ImagingFactory() {
    static ComPtr<IWICImagingFactory> factory;
    if (factory != nullptr) {
        return Result<IWICImagingFactory*>::Ok(factory.Get());
    }

    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(result) && result != RPC_E_CHANGED_MODE) {
        return Result<IWICImagingFactory*>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CoInitializeEx", result)});
    }

    result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(result)) {
        return Result<IWICImagingFactory*>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CoCreateInstance(WIC)", result)});
    }
    return Result<IWICImagingFactory*>::Ok(factory.Get());
}

Result<std::wstring> WidePath(std::string_view path) {
    if (path.empty()) {
        return Result<std::wstring>::Fail(
            Error{ErrorCode::InvalidArgument, "image path is empty"});
    }
    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        path.data(),
        static_cast<int>(path.size()),
        nullptr,
        0);
    if (length <= 0) {
        return Result<std::wstring>::Fail(
            Error{ErrorCode::InvalidArgument, "image path is not valid UTF-8"});
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        path.data(),
        static_cast<int>(path.size()),
        wide.data(),
        length);
    return Result<std::wstring>::Ok(std::move(wide));
}

}  // namespace

Result<void> SavePng(
    std::string_view utf8_path,
    int width,
    int height,
    std::span<const std::uint32_t> pixels) noexcept {
    if (width <= 0 || height <= 0) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidArgument, "image size must be positive"});
    }
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pixels.size() != expected) {
        return Result<void>::Fail(
            Error{ErrorCode::InvalidArgument, "pixel count does not match image size"});
    }

    auto wide_result = WidePath(utf8_path);
    if (!wide_result.ok()) {
        return Result<void>::Fail(wide_result.error());
    }
    const std::wstring& wide = wide_result.value();

    auto factory_result = ImagingFactory();
    if (!factory_result.ok()) {
        return Result<void>::Fail(factory_result.error());
    }
    IWICImagingFactory* factory = factory_result.value();

    // WIC opens an existing file instead of truncating it; drop it first.
    DeleteFileW(wide.c_str());

    ComPtr<IWICStream> stream;
    HRESULT result = factory->CreateStream(stream.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateStream", result)});
    }
    result = stream->InitializeFromFilename(wide.c_str(), GENERIC_WRITE);
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("InitializeFromFilename", result)});
    }

    ComPtr<IWICBitmapEncoder> encoder;
    result = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateEncoder", result)});
    }
    result = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("Encoder::Initialize", result)});
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    result = encoder->CreateNewFrame(frame.GetAddressOf(), properties.GetAddressOf());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateNewFrame", result)});
    }
    result = frame->Initialize(properties.Get());
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("Frame::Initialize", result)});
    }
    result = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("SetSize", result)});
    }

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    result = frame->SetPixelFormat(&format);
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("SetPixelFormat", result)});
    }
    if (!IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) {
        return Result<void>::Fail(
            Error{ErrorCode::Unsupported, "PNG encoder rejected 32bpp BGRA output"});
    }

    const std::size_t byte_count = expected * sizeof(std::uint32_t);
    const std::size_t stride = static_cast<std::size_t>(width) * sizeof(std::uint32_t);
    result = frame->WritePixels(
        static_cast<UINT>(height),
        static_cast<UINT>(stride),
        static_cast<UINT>(byte_count),
        reinterpret_cast<BYTE*>(const_cast<std::uint32_t*>(pixels.data())));
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("WritePixels", result)});
    }

    result = frame->Commit();
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("Frame::Commit", result)});
    }
    result = encoder->Commit();
    if (FAILED(result)) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("Encoder::Commit", result)});
    }
    return Result<void>::Ok();
}

Result<Image> LoadPng(std::string_view utf8_path) noexcept {
    auto wide_result = WidePath(utf8_path);
    if (!wide_result.ok()) {
        return Result<Image>::Fail(wide_result.error());
    }
    const std::wstring& wide = wide_result.value();

    auto factory_result = ImagingFactory();
    if (!factory_result.ok()) {
        return Result<Image>::Fail(factory_result.error());
    }
    IWICImagingFactory* factory = factory_result.value();

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT result = factory->CreateDecoderFromFilename(
        wide.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        decoder.GetAddressOf());
    if (FAILED(result)) {
        return Result<Image>::Fail(Error{
            ErrorCode::NotFound,
            HResultMessage("CreateDecoderFromFilename", result)});
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(result)) {
        return Result<Image>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("GetFrame", result)});
    }

    UINT width = 0;
    UINT height = 0;
    result = frame->GetSize(&width, &height);
    if (FAILED(result)) {
        return Result<Image>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("GetSize", result)});
    }
    if (width == 0 || height == 0) {
        return Result<Image>::Fail(
            Error{ErrorCode::InvalidState, "image has an empty size"});
    }

    ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result)) {
        return Result<Image>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CreateFormatConverter", result)});
    }
    result = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(result)) {
        return Result<Image>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("Converter::Initialize", result)});
    }

    Image image;
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.pixels.resize(
        static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height));

    const UINT stride = width * sizeof(std::uint32_t);
    const UINT byte_count = stride * height;
    result = converter->CopyPixels(
        nullptr,
        stride,
        byte_count,
        reinterpret_cast<BYTE*>(image.pixels.data()));
    if (FAILED(result)) {
        return Result<Image>::Fail(
            Error{ErrorCode::PlatformFailure, HResultMessage("CopyPixels", result)});
    }
    return Result<Image>::Ok(std::move(image));
}

}  // namespace nopeui::graphics
