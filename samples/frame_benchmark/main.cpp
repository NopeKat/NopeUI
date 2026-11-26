#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include "nopeui/platform/window.h"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

int main(int argc, char** argv) {
    bool paced = true;
    bool full_frame = false;
    bool throttle_commits = true;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--paced") == 0) {
            paced = true;
        } else if (std::strcmp(argv[index], "--unpaced") == 0) {
            paced = false;
        } else if (std::strcmp(argv[index], "--full") == 0) {
            full_frame = true;
        } else if (std::strcmp(argv[index], "--no-throttle") == 0) {
            throttle_commits = false;
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
    window->SetCommitThrottle(throttle_commits);

    const double frame_ms = paced ? 1000.0 / 120.0 : 0.0;
    HANDLE frame_timer = nullptr;
    if (paced) {
        frame_timer = CreateWaitableTimerExW(
            nullptr,
            nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS);
        if (frame_timer == nullptr) {
            std::fprintf(stderr, "high-resolution timer unavailable; using Sleep\n");
        }
    }

    const auto wait_for_frame = [&] {
        if (frame_timer != nullptr) {
            LARGE_INTEGER due{};
            due.QuadPart = -static_cast<LONGLONG>(frame_ms * 10000.0);
            if (SetWaitableTimer(frame_timer, &due, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(frame_timer, INFINITE);
                return;
            }
        }
        if (paced) {
            Sleep(static_cast<DWORD>(frame_ms));
        }
    };

    const auto render_frame = [&] {
        if (full_frame) {
            window->RenderFrame();
        } else {
            window->RenderFrame(nopeui::Rect{0.0f, 0.0f, 900.0f, 4.0f});
        }
    };

    constexpr int warmup_frames = 30;
    constexpr int measured_frames = 300;
    for (int index = 0; index < warmup_frames; ++index) {
        wait_for_frame();
        render_frame();
    }

    std::vector<double> frame_times;
    frame_times.reserve(measured_frames);
    int attempts = 0;
    const int max_attempts = measured_frames * 4;
    while (static_cast<int>(frame_times.size()) < measured_frames &&
           attempts < max_attempts) {
        wait_for_frame();

        const auto commits_before = window->CommitCount();
        const auto start = std::chrono::steady_clock::now();
        render_frame();
        const auto elapsed = std::chrono::steady_clock::now() - start;

        if (window->CommitCount() != commits_before) {
            frame_times.push_back(
                std::chrono::duration<double, std::milli>(elapsed).count());
        }
        ++attempts;
    }

    if (frame_times.empty()) {
        std::fprintf(stderr, "no committed frames measured\n");
        if (frame_timer != nullptr) {
            CloseHandle(frame_timer);
        }
        window->Close();
        return 1;
    }

    const auto commit_count = window->CommitCount();

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
        "content=%s pace=%s throttle=%s target_hz=%.0f frames=%zu attempts=%d "
        "commits=%llu avg=%.3f ms p99=%.3f ms min=%.3f ms max=%.3f ms\n",
        full_frame ? "full" : "dirty",
        paced ? "fixed" : "unpaced",
        throttle_commits ? "on" : "off",
        paced ? 1000.0 / frame_ms : 0.0,
        frame_times.size(),
        attempts,
        static_cast<unsigned long long>(commit_count),
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

    if (frame_timer != nullptr) {
        CloseHandle(frame_timer);
    }
    window->Close();
    return 0;
}
