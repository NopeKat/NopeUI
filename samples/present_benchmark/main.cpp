#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {

constexpr int kWarmupFrames = 30;
constexpr int kMeasuredFrames = 300;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

void PumpMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

bool Check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::fprintf(stderr, "%s failed: 0x%08lX\n", operation, static_cast<unsigned long>(result));
        return false;
    }
    return true;
}

std::pair<double, double> Summarize(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const double average =
        std::accumulate(values.begin(), values.end(), 0.0) /
        static_cast<double>(values.size());
    const double p99 = values[static_cast<std::size_t>(
        static_cast<double>(values.size() - 1) * 0.99)];
    return {average, p99};
}

bool CreateDevice(ComPtr<ID3D11Device>& device, ComPtr<IDXGIDevice>& dxgi_device) {
    constexpr D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL selected{};
    HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,
        static_cast<UINT>(std::size(levels)),
        D3D11_SDK_VERSION,
        device.GetAddressOf(),
        &selected,
        nullptr);
    if (FAILED(result)) {
        result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels,
            static_cast<UINT>(std::size(levels)),
            D3D11_SDK_VERSION,
            device.GetAddressOf(),
            &selected,
            nullptr);
    }
    if (!Check(result, "D3D11CreateDevice")) {
        return false;
    }
    return Check(device.As(&dxgi_device), "Query IDXGIDevice");
}

bool CreateComposition(
    HWND hwnd,
    IDXGIDevice* dxgi_device,
    ComPtr<IDCompositionDesktopDevice>& dcomp,
    ComPtr<IDCompositionVisual2>& visual) {
    if (!Check(
            DCompositionCreateDevice3(dxgi_device, IID_PPV_ARGS(dcomp.GetAddressOf())),
            "DCompositionCreateDevice3")) {
        return false;
    }
    ComPtr<IDCompositionTarget> target;
    if (!Check(
            dcomp->CreateTargetForHwnd(hwnd, TRUE, target.GetAddressOf()),
            "CreateTargetForHwnd")) {
        return false;
    }
    if (!Check(dcomp->CreateVisual(visual.GetAddressOf()), "CreateVisual")) {
        return false;
    }
    return Check(target->SetRoot(visual.Get()), "SetRoot");
}

bool CreateRenderTarget(
    ID3D11Device* device,
    IDXGISurface* surface,
    ComPtr<ID3D11RenderTargetView>& view) {
    ComPtr<ID3D11Texture2D> texture;
    if (!Check(
            surface->QueryInterface(IID_PPV_ARGS(texture.GetAddressOf())),
            "Query ID3D11Texture2D")) {
        return false;
    }
    D3D11_RENDER_TARGET_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    return Check(
        device->CreateRenderTargetView(texture.Get(), &desc, view.GetAddressOf()),
        "CreateRenderTargetView");
}

} // namespace

int main(int argc, char** argv) {
    int width = 3840;
    int height = 2160;
    bool flip = false;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--width") == 0 && index + 1 < argc) {
            width = std::atoi(argv[++index]);
        } else if (std::strcmp(argv[index], "--height") == 0 && index + 1 < argc) {
            height = std::atoi(argv[++index]);
        } else if (std::strcmp(argv[index], "--flip") == 0) {
            flip = true;
        }
    }
    if (width <= 0 || height <= 0) {
        std::fprintf(stderr, "invalid size\n");
        return 1;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    window_class.lpszClassName = L"NopeUIPresentBenchmark";
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        std::fprintf(stderr, "RegisterClassExW failed: %lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        window_class.lpszClassName,
        L"NopeUI Present Benchmark",
        WS_POPUP | WS_VISIBLE,
        0,
        0,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (hwnd == nullptr) {
        std::fprintf(stderr, "CreateWindowExW failed: %lu\n", GetLastError());
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    ComPtr<ID3D11Device> device;
    ComPtr<IDXGIDevice> dxgi_device;
    if (!CreateDevice(device, dxgi_device)) {
        return 1;
    }

    ComPtr<IDCompositionDesktopDevice> dcomp;
    ComPtr<IDCompositionVisual2> visual;
    if (!CreateComposition(hwnd, dxgi_device.Get(), dcomp, visual)) {
        return 1;
    }

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.GetAddressOf());

    ComPtr<IDCompositionVirtualSurface> virtual_surface;
    ComPtr<IDXGISwapChain1> swap_chain;
    ComPtr<ID3D11RenderTargetView> render_target;
    if (flip) {
        ComPtr<IDXGIFactory1> factory1;
        if (!Check(CreateDXGIFactory1(IID_PPV_ARGS(factory1.GetAddressOf())),
                   "CreateDXGIFactory1")) {
            return 1;
        }
        ComPtr<IDXGIFactory2> factory2;
        if (!Check(factory1.As(&factory2), "Query IDXGIFactory2")) {
            return 1;
        }
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        if (!Check(
                factory2->CreateSwapChainForComposition(
                    device.Get(), &desc, nullptr, swap_chain.GetAddressOf()),
                "CreateSwapChainForComposition")) {
            return 1;
        }
        if (!Check(visual->SetContent(swap_chain.Get()), "SetContent(swap chain)")) {
            return 1;
        }
        if (!Check(dcomp->Commit(), "Commit(swap chain)")) {
            return 1;
        }
    } else {
        if (!Check(
                dcomp->CreateVirtualSurface(
                    static_cast<UINT>(width),
                    static_cast<UINT>(height),
                    DXGI_FORMAT_B8G8R8A8_UNORM,
                    DXGI_ALPHA_MODE_PREMULTIPLIED,
                    virtual_surface.GetAddressOf()),
                "CreateVirtualSurface")) {
            return 1;
        }
        if (!Check(visual->SetContent(virtual_surface.Get()), "SetContent(virtual surface)")) {
            return 1;
        }
    }

    std::vector<double> frame_times;
    frame_times.reserve(kMeasuredFrames);

    for (int index = 0; index < kWarmupFrames + kMeasuredFrames; ++index) {
        PumpMessages();
        ComPtr<IDXGISurface> surface;
        double elapsed = 0.0;
        if (flip) {
            if (!Check(swap_chain->GetBuffer(0, IID_PPV_ARGS(surface.GetAddressOf())),
                       "GetBuffer")) {
                return 1;
            }
            render_target.Reset();
            if (!CreateRenderTarget(device.Get(), surface.Get(), render_target)) {
                return 1;
            }
            const float color[] = {0.10f, 0.10f, 0.10f, 1.0f};
            context->ClearRenderTargetView(render_target.Get(), color);
            const auto start = std::chrono::steady_clock::now();
            HRESULT result = swap_chain->Present(0, 0);
            elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start)
                          .count();
            if (!Check(result, "Present")) {
                return 1;
            }
        } else {
            POINT offset{};
            if (!Check(
                    virtual_surface->BeginDraw(
                        nullptr, IID_PPV_ARGS(surface.GetAddressOf()), &offset),
                    "BeginDraw")) {
                return 1;
            }
            render_target.Reset();
            if (!CreateRenderTarget(device.Get(), surface.Get(), render_target)) {
                return 1;
            }
            const float color[] = {0.10f, 0.10f, 0.10f, 1.0f};
            context->ClearRenderTargetView(render_target.Get(), color);
            if (!Check(virtual_surface->EndDraw(), "EndDraw")) {
                return 1;
            }
            const auto start = std::chrono::steady_clock::now();
            HRESULT result = dcomp->Commit();
            elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start)
                          .count();
            if (!Check(result, "Commit")) {
                return 1;
            }
        }
        if (index >= kWarmupFrames) {
            frame_times.push_back(elapsed);
        }
        Sleep(4);
    }

    const auto [average, p99] = Summarize(std::move(frame_times));
    RECT client{};
    GetClientRect(hwnd, &client);
    std::printf(
        "present mode=%s requested=%dx%d client=%dx%d avg=%.3f ms p99=%.3f ms\n",
        flip ? "flip" : "virtual",
        width,
        height,
        client.right - client.left,
        client.bottom - client.top,
        average,
        p99);

    dcomp->Commit();
    DestroyWindow(hwnd);
    return 0;
}
