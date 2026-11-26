#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include "nopeui/platform/window.h"

int main(int argc, char** argv) {
    bool async = true;
    bool flip = false;
    int interval_ms = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--sync") == 0) {
            async = false;
        } else if (std::strcmp(argv[index], "--flip") == 0) {
            flip = true;
        } else if (std::strcmp(argv[index], "--interval") == 0 && index + 1 < argc) {
            interval_ms = std::atoi(argv[++index]);
            if (interval_ms < 0) {
                interval_ms = 0;
            }
        }
    }

    auto result = nopeui::Window::Create(900, 600, u"NopeUI Resize Benchmark");
    if (!result.ok()) {
        std::fprintf(stderr, "Window::Create failed: %s\n", result.error().message.c_str());
        return 1;
    }

    auto window = std::move(result).value();
    window->Show();
    if (flip) {
        window->SetDiagnosticFlipModel(true);
    }
    window->SetAsyncCommit(async);

    const auto resize_frame = [&](int width,
                                  int height,
                                  std::vector<double>* set_times,
                                  std::vector<double>* render_times,
                                  std::vector<double>* total_times,
                                  std::vector<double>* resize_times,
                                  std::vector<double>* draw_times,
                                  std::vector<double>* frame_times,
                                  std::vector<double>* begin_times,
                                  std::vector<double>* end_times) {
        const auto start = std::chrono::steady_clock::now();
        SetWindowPos(
            window->Handle(),
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        const auto after_set = std::chrono::steady_clock::now();
        window->RenderFrame();
        const auto end = std::chrono::steady_clock::now();
        if (set_times != nullptr) {
            set_times->push_back(
                std::chrono::duration<double, std::milli>(after_set - start).count());
            render_times->push_back(
                std::chrono::duration<double, std::milli>(end - after_set).count());
            total_times->push_back(
                std::chrono::duration<double, std::milli>(end - start).count());
            resize_times->push_back(window->LastResizeMilliseconds());
            const auto timings = window->LastFrameTimings();
            draw_times->push_back(timings.draw_ms);
            frame_times->push_back(window->LastFrameMilliseconds());
            begin_times->push_back(timings.begin_draw_ms);
            end_times->push_back(timings.end_draw_ms);
        }
    };

    for (int index = 0; index < 20; ++index) {
        resize_frame(
            900 + (index % 5) * 40,
            600 + (index % 5) * 30,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr);
        if (interval_ms > 0) {
            Sleep(static_cast<DWORD>(interval_ms));
        }
    }

    constexpr int measured = 100;
    std::vector<double> set_times;
    std::vector<double> render_times;
    std::vector<double> total_times;
    std::vector<double> resize_phase_times;
    std::vector<double> draw_phase_times;
    std::vector<double> frame_phase_times;
    std::vector<double> begin_phase_times;
    std::vector<double> end_phase_times;
    set_times.reserve(measured);
    render_times.reserve(measured);
    total_times.reserve(measured);
    resize_phase_times.reserve(measured);
    draw_phase_times.reserve(measured);
    frame_phase_times.reserve(measured);
    begin_phase_times.reserve(measured);
    end_phase_times.reserve(measured);
    for (int index = 0; index < measured; ++index) {
        const int width = 720 + (index % 11) * 40;
        const int height = 480 + (index % 11) * 30;
        resize_frame(
            width,
            height,
            &set_times,
            &render_times,
            &total_times,
            &resize_phase_times,
            &draw_phase_times,
            &frame_phase_times,
            &begin_phase_times,
            &end_phase_times);
        if (interval_ms > 0) {
            Sleep(static_cast<DWORD>(interval_ms));
        }
    }

    const auto summarize = [](std::vector<double> values) {
        std::sort(values.begin(), values.end());
        const double average =
            std::accumulate(values.begin(), values.end(), 0.0) /
            static_cast<double>(values.size());
        const double p99 = values[static_cast<std::size_t>(values.size() * 0.99)];
        return std::tuple{average, p99, values.back()};
    };
    const auto [set_average, set_p99, set_max] = summarize(std::move(set_times));
    const auto [render_average, render_p99, render_max] = summarize(std::move(render_times));
    const auto [resize_average, resize_p99, resize_max] =
        summarize(std::move(resize_phase_times));
    const auto [draw_average, draw_p99, draw_max] = summarize(std::move(draw_phase_times));
    const auto [frame_average, frame_p99, frame_max] = summarize(std::move(frame_phase_times));
    const auto [begin_average, begin_p99, begin_max] = summarize(std::move(begin_phase_times));
    const auto [end_average, end_p99, end_max] = summarize(std::move(end_phase_times));
    const auto [all_average, all_p99, all_max] = summarize(std::move(total_times));
    window->SetAsyncCommit(false);
    const auto stats = window->AsyncCommitStats();
    std::printf(
        "resize async=%s mode=%s interval=%d samples=%d "
        "set=%.3f/%.3f/%.3f render=%.3f/%.3f/%.3f "
        "phase_resize=%.3f/%.3f/%.3f draw=%.3f/%.3f/%.3f "
        "frame=%.3f/%.3f/%.3f begin=%.3f/%.3f/%.3f end=%.3f/%.3f/%.3f "
        "total=%.3f/%.3f/%.3f "
        "submitted=%llu completed=%llu dropped=%llu waits=%llu wait_avg=%.3f maxq=%u\n",
        window->IsAsyncCommitEnabled() ? "on" : "off",
        flip ? "flip" : "virtual",
        interval_ms,
        measured,
        set_average,
        set_p99,
        set_max,
        render_average,
        render_p99,
        render_max,
        resize_average,
        resize_p99,
        resize_max,
        draw_average,
        draw_p99,
        draw_max,
        frame_average,
        frame_p99,
        frame_max,
        begin_average,
        begin_p99,
        begin_max,
        end_average,
        end_p99,
        end_max,
        all_average,
        all_p99,
        all_max,
        static_cast<unsigned long long>(stats.submitted),
        static_cast<unsigned long long>(stats.completed),
        static_cast<unsigned long long>(stats.dropped),
        static_cast<unsigned long long>(stats.waits),
        stats.waits > 0 ? stats.total_wait_ms / static_cast<double>(stats.waits) : 0.0,
        stats.max_depth);

    window->Close();
    return 0;
}
