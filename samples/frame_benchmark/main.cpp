#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <dwmapi.h>

#include "nopeui/platform/window.h"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

int main(int argc, char** argv) {
    bool paced = true;
    bool full_frame = false;
    bool throttle_commits = true;
    bool async_commit = false;
    bool diagnostic_virtual = false;
    double diagnostic_scroll_step = 0.0;
    double target_hz = 120.0;
    std::uint32_t diagnostic_rectangles = 0;
    std::uint32_t diagnostic_text_runs = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--paced") == 0) {
            paced = true;
        } else if (std::strcmp(argv[index], "--unpaced") == 0) {
            paced = false;
        } else if (std::strcmp(argv[index], "--full") == 0) {
            full_frame = true;
        } else if (std::strcmp(argv[index], "--no-throttle") == 0) {
            throttle_commits = false;
        } else if (std::strcmp(argv[index], "--async-commit") == 0) {
            async_commit = true;
        } else if (std::strcmp(argv[index], "--virtual") == 0) {
            diagnostic_virtual = true;
        } else if (std::strcmp(argv[index], "--scroll") == 0 && index + 1 < argc) {
            diagnostic_scroll_step = std::strtod(argv[++index], nullptr);
        } else if (std::strcmp(argv[index], "--rects") == 0 && index + 1 < argc) {
            diagnostic_rectangles = static_cast<std::uint32_t>(std::strtoul(argv[++index], nullptr, 10));
        } else if (std::strcmp(argv[index], "--text") == 0 && index + 1 < argc) {
            diagnostic_text_runs = static_cast<std::uint32_t>(std::strtoul(argv[++index], nullptr, 10));
        } else if (std::strcmp(argv[index], "--hz") == 0 && index + 1 < argc) {
            const double value = std::strtod(argv[++index], nullptr);
            if (value > 0.0) {
                target_hz = value;
            }
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
    window->SetAsyncCommit(async_commit);
    window->SetDiagnosticLoad(diagnostic_rectangles, diagnostic_text_runs, diagnostic_virtual);

    DWM_TIMING_INFO composition_timing{};
    composition_timing.cbSize = sizeof(composition_timing);
    double dwm_refresh_hz = 0.0;
    if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &composition_timing))) {
        if (composition_timing.rateRefresh.uiDenominator != 0) {
            dwm_refresh_hz =
                static_cast<double>(composition_timing.rateRefresh.uiNumerator) /
                static_cast<double>(composition_timing.rateRefresh.uiDenominator);
        }
        if (dwm_refresh_hz == 0.0 && composition_timing.qpcRefreshPeriod != 0) {
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            dwm_refresh_hz = static_cast<double>(frequency.QuadPart) /
                             static_cast<double>(composition_timing.qpcRefreshPeriod);
        }
    }

    const double frame_ms = paced ? 1000.0 / target_hz : 0.0;
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

    double diagnostic_scroll = 0.0;
    const auto render_frame = [&] {
        if (diagnostic_scroll_step != 0.0) {
            diagnostic_scroll += diagnostic_scroll_step;
            window->SetDiagnosticScroll(static_cast<float>(diagnostic_scroll));
        }
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
    const auto commits_before_measurement = window->CommitCount();

    std::vector<double> frame_times;
    std::vector<double> begin_times;
    std::vector<double> draw_times;
    std::vector<double> end_times;
    std::vector<double> commit_times;
    std::vector<double> queue_times;
    std::vector<double> present_times;
    std::vector<double> latency_commit_times;
    frame_times.reserve(measured_frames);
    begin_times.reserve(measured_frames);
    draw_times.reserve(measured_frames);
    end_times.reserve(measured_frames);
    commit_times.reserve(measured_frames);
    queue_times.reserve(measured_frames);
    present_times.reserve(measured_frames);
    latency_commit_times.reserve(measured_frames);
    std::uint64_t last_latency_sequence = window->LastPresentedFrame().sequence;
    const auto collect_latency = [&] {
        const auto latency = window->LastPresentedFrame();
        if (latency.sequence == 0 || latency.sequence == last_latency_sequence) {
            return;
        }
        last_latency_sequence = latency.sequence;
        queue_times.push_back(latency.queue_ms);
        present_times.push_back(latency.present_ms);
        latency_commit_times.push_back(latency.commit_ms);
    };
    int attempts = 0;
    const int max_attempts = measured_frames * 4;
    while (static_cast<int>(frame_times.size()) < measured_frames &&
           attempts < max_attempts) {
        wait_for_frame();

        const auto commits_before = window->CommitCount();
        const auto start = std::chrono::steady_clock::now();
        render_frame();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        collect_latency();

        if (async_commit || window->CommitCount() != commits_before) {
            const auto timings = window->LastFrameTimings();
            frame_times.push_back(
                std::chrono::duration<double, std::milli>(elapsed).count());
            begin_times.push_back(timings.begin_draw_ms);
            draw_times.push_back(timings.draw_ms);
            end_times.push_back(timings.end_draw_ms);
            commit_times.push_back(timings.commit_ms);
        }
        ++attempts;
    }

    double drain_ms = 0.0;
    if (async_commit) {
        const auto drain_start = std::chrono::steady_clock::now();
        const auto target_commits =
            commits_before_measurement + static_cast<std::uint64_t>(measured_frames);
        while (window->CommitCount() < target_commits) {
            const auto elapsed = std::chrono::steady_clock::now() - drain_start;
            if (elapsed > std::chrono::seconds(2)) {
                break;
            }
            Sleep(1);
        }
        drain_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - drain_start).count();
        collect_latency();
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

    const auto summarize = [](std::vector<double> values) {
        if (values.empty()) {
            return std::pair<double, double>{0.0, 0.0};
        }
        std::sort(values.begin(), values.end());
        double total = 0.0;
        for (const double value : values) {
            total += value;
        }
        const double average = total / static_cast<double>(values.size());
        const auto p99_index = static_cast<std::size_t>(
            static_cast<double>(values.size() - 1) * 0.99);
        return std::pair<double, double>{average, values[p99_index]};
    };

    const auto frame_summary = summarize(frame_times);
    const auto begin_summary = summarize(begin_times);
    const auto draw_summary = summarize(draw_times);
    const auto end_summary = summarize(end_times);
    const auto commit_summary = summarize(commit_times);
    const auto queue_summary = summarize(queue_times);
    const auto latency_commit_summary = summarize(latency_commit_times);
    const auto present_summary = summarize(present_times);
    const auto stats = window->AsyncCommitStats();
    const auto commit_display = async_commit ? latency_commit_summary : commit_summary;

    const double average = frame_summary.first;
    const double p99 = frame_summary.second;

    std::printf(
        "content=%s pace=%s throttle=%s async=%s target_hz=%.0f dwm_refresh=%.1f "
        "load_rects=%u load_text=%u virtual=%d scroll=%.2f "
        "frames=%zu attempts=%d "
        "commits=%llu avg=%.3f ms p99=%.3f ms min=%.3f ms max=%.3f ms "
        "drain=%.1f ms begin=%.3f/%.3f draw=%.3f/%.3f end=%.3f/%.3f commit=%.3f/%.3f "
        "queue=%.3f/%.3f present=%.3f/%.3f "
        "submitted=%llu completed=%llu dropped=%llu waits=%llu wait_avg=%.3f maxq=%u\n",
        full_frame ? "full" : "dirty",
        paced ? "fixed" : "unpaced",
        throttle_commits ? "on" : "off",
        async_commit ? "on" : "off",
        paced ? target_hz : 0.0,
        dwm_refresh_hz,
        diagnostic_rectangles,
        diagnostic_text_runs,
        diagnostic_virtual ? 1 : 0,
        diagnostic_scroll_step,
        frame_times.size(),
        attempts,
        static_cast<unsigned long long>(commit_count),
        average,
        p99,
        *std::min_element(frame_times.begin(), frame_times.end()),
        *std::max_element(frame_times.begin(), frame_times.end()),
        drain_ms,
        begin_summary.first,
        begin_summary.second,
        draw_summary.first,
        draw_summary.second,
        end_summary.first,
        end_summary.second,
        commit_display.first,
        commit_display.second,
        queue_summary.first,
        queue_summary.second,
        present_summary.first,
        present_summary.second,
        static_cast<unsigned long long>(stats.submitted),
        static_cast<unsigned long long>(stats.completed),
        static_cast<unsigned long long>(stats.dropped),
        static_cast<unsigned long long>(stats.waits),
        stats.waits > 0 ? stats.total_wait_ms / static_cast<double>(stats.waits) : 0.0,
        stats.max_depth);

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
