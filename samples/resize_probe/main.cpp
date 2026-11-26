#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

#include <Windows.h>

namespace {

constexpr const wchar_t* kWindowClassName = L"NopeUI.ResizeProbe.Plain";

LRESULT CALLBACK ProbeWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

int main() {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &ProbeWindowProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&window_class) == 0) {
        std::fprintf(stderr, "RegisterClassExW failed: %lu\n", GetLastError());
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        kWindowClassName,
        L"NopeUI Plain Resize Probe",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        900,
        600,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "CreateWindowExW failed: %lu\n", GetLastError());
        return 1;
    }

    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);
    Sleep(500);

    constexpr int resize_count = 100;
    std::vector<double> times;
    times.reserve(resize_count);
    for (int index = 0; index < resize_count; ++index) {
        const int width = 700 + (index % 40) * 5;
        const int height = 500 + (index % 40) * 3;
        const auto start = std::chrono::steady_clock::now();
        SetWindowPos(
            window,
            nullptr,
            100,
            100,
            width,
            height,
            SWP_NOZORDER | SWP_NOACTIVATE);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        times.push_back(std::chrono::duration<double, std::milli>(elapsed).count());
    }

    std::sort(times.begin(), times.end());
    double total = 0.0;
    for (const double time : times) {
        total += time;
    }
    const double average = total / static_cast<double>(times.size());
    const auto p99_index =
        static_cast<std::size_t>(static_cast<double>(times.size() - 1) * 0.99);

    std::printf(
        "plain resize count=%zu avg=%.3f ms p99=%.3f ms min=%.3f ms max=%.3f ms\n",
        times.size(),
        average,
        times[p99_index],
        times.front(),
        times.back());

    DestroyWindow(window);
    return 0;
}
