#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <utility>
#include <vector>

#include <Windows.h>
#include <dwmapi.h>

#include "nopeui/platform/window.h"

int main(int argc, char** argv) {
    bool paced = false;
    bool full_frame = false;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--paced") == 0) {
            paced = true;
        } else if (std::strcmp(argv[index], "--unpaced") == 0) {
            paced = false;
        } else if (std::strcmp(argv[index], "--full") == 0) {
            full_frame = true;
        }
    }

    auto result = nopeui::Window::Create(900, 600, u"NopeUI Frame Benchmark");
    if (!result.ok()) {
        std::fprintf(
            stderr,
            "Window::Create failed: code=%u message=%s\n",
            static_cast<unsigned int>(result.error().code),
            result.error().message.c_str());
        return 1;
    }

    auto window = std::move(result).value();
    window->Show();

    constexpr int warmup_frames = 30;
    constexpr int measured_frames = 300;
    const nopeui::Rect dirty_rect{0.0f, 0.0f, 900.0f, 4.0f};
    for (int index = 0; index < warmup_frames; ++index) {
        if (full_frame) {
            window->RenderFrame();
        } else {
            window->RenderFrame(dirty_rect);
        }
    }

    std::vector<double> frame_times;
    frame_times.reserve(measured_frames);
    for (int index = 0; index < measured_frames; ++index) {
        if (paced) {
            DwmFlush();
        }
        if (full_frame) {
            window->RenderFrame();
        } else {
            window->RenderFrame(dirty_rect);
        }
        frame_times.push_back(window->LastFrameMilliseconds());
    }

    std::sort(frame_times.begin(), frame_times.end());
    double total = 0.0;
    for (const double frame_time : frame_times) {
        total += frame_time;
    }

    const double average = total / static_cast<double>(frame_times.size());
    const auto p99_index = static_cast<std::size_t>(
        static_cast<double>(frame_times.size() - 1) * 0.99);
    const double p99 = frame_times[p99_index];

    std::printf(
        "content=%s pace=%s frames=%zu avg=%.3f ms p99=%.3f ms min=%.3f ms max=%.3f ms\n",
        full_frame ? "full" : "dirty",
        paced ? "dwm-synced" : "unpaced",
        frame_times.size(),
        average,
        p99,
        frame_times.front(),
        frame_times.back());

    wchar_t title[160]{};
    std::swprintf(
        title,
        std::size(title),
        L"NopeUI Frame Benchmark - avg %.2f ms / p99 %.2f ms",
        average,
        p99);
    SetWindowTextW(window->Handle(), title);

    window->Close();
    return 0;
}
