#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include "nopeui/platform/window.h"

int main(int argc, char** argv) {
    bool async = true;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--sync") == 0) {
            async = false;
        }
    }

    auto result = nopeui::Window::Create(900, 600, u"NopeUI Resize Benchmark");
    if (!result.ok()) {
        std::fprintf(stderr, "Window::Create failed: %s\n", result.error().message.c_str());
        return 1;
    }

    auto window = std::move(result).value();
    window->Show();
    window->SetAsyncCommit(async);

    const auto resize_frame = [&](int width, int height) {
        SetWindowPos(
            window->Handle(),
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        window->RenderFrame();
    };

    for (int index = 0; index < 20; ++index) {
        resize_frame(900 + (index % 5) * 40, 600 + (index % 5) * 30);
    }

    constexpr int measured = 100;
    std::vector<double> times;
    times.reserve(measured);
    for (int index = 0; index < measured; ++index) {
        const int width = 720 + (index % 11) * 40;
        const int height = 480 + (index % 11) * 30;
        const auto start = std::chrono::steady_clock::now();
        resize_frame(width, height);
        times.push_back(std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count());
    }

    std::sort(times.begin(), times.end());
    const double average =
        std::accumulate(times.begin(), times.end(), 0.0) / static_cast<double>(times.size());
    const double p99 = times[static_cast<std::size_t>(times.size() * 0.99)];
    window->SetAsyncCommit(false);
    const auto stats = window->AsyncCommitStats();
    std::printf(
        "resize async=%s samples=%zu avg=%.3f p99=%.3f max=%.3f "
        "submitted=%llu completed=%llu dropped=%llu waits=%llu wait_avg=%.3f maxq=%u\n",
        async ? "on" : "off",
        times.size(),
        average,
        p99,
        times.back(),
        static_cast<unsigned long long>(stats.submitted),
        static_cast<unsigned long long>(stats.completed),
        static_cast<unsigned long long>(stats.dropped),
        static_cast<unsigned long long>(stats.waits),
        stats.waits > 0 ? stats.total_wait_ms / static_cast<double>(stats.waits) : 0.0,
        stats.max_depth);

    window->Close();
    return 0;
}
