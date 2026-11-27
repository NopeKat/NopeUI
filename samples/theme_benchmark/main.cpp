#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include "form_page.h"
#include "nopeui/platform/window.h"
#include "nopeui/theme/theme.h"
#include "perf_metrics.h"

int main(int argc, char** argv) {
    using namespace nopeui::samples;

    int switches = 200;
    bool async_commit = true;
    bool software = false;
    bool throttle = false;
    bool contrast = false;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--switches") == 0 && index + 1 < argc) {
            const int value = std::atoi(argv[++index]);
            if (value > 0) {
                switches = value;
            }
        } else if (std::strcmp(argv[index], "--sync") == 0) {
            async_commit = false;
        } else if (std::strcmp(argv[index], "--software") == 0) {
            software = true;
        } else if (std::strcmp(argv[index], "--throttle") == 0) {
            throttle = true;
        } else if (std::strcmp(argv[index], "--contrast") == 0) {
            contrast = true;
        }
    }

    auto created = nopeui::Window::Create(900, 640, u"NopeUI Theme Benchmark");
    if (!created.ok()) {
        std::fprintf(stderr, "Window::Create failed: %s\n", created.error().message.c_str());
        return 1;
    }

    Clock clock;
    auto window = std::move(created).value();
    window->SetContent(BuildFormPage());
    window->SetStyleSheet(BuildFormStyleSheet());
    window->SetTheme(nopeui::LightTheme());
    window->SetAsyncCommit(async_commit);
    // One commit per refresh would put a vblank wait inside every sample; the
    // switch cost is what matters here, so throttling stays off by default.
    window->SetCommitThrottle(throttle);
    if (software) {
        window->SetSoftwareRender(true);
    }
    window->Show();

    int palette = 0;
    const int palette_count = contrast ? 3 : 2;
    const auto next_theme = [&palette, palette_count]() -> nopeui::Theme {
        const nopeui::Theme* theme = &nopeui::LightTheme();
        const int index = palette % palette_count;
        if (index == 1) {
            theme = &nopeui::DarkTheme();
        } else if (index == 2) {
            theme = &nopeui::HighContrastTheme();
        }
        ++palette;
        return *theme;
    };

    // Spins until the commit that carries the switch is finished, so apply,
    // frame and settle are three separate numbers.
    const auto finish_frame = [&window](std::uint64_t commits_before,
                                        std::uint64_t completed_before) {
        const auto start = std::chrono::steady_clock::now();
        const bool async = window->IsAsyncCommitEnabled();
        while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000)) {
            const auto stats = window->AsyncCommitStats();
            if (window->CommitCount() > commits_before &&
                (!async || stats.completed > completed_before)) {
                return std::chrono::steady_clock::now();
            }
            SwitchToThread();
        }
        return std::chrono::steady_clock::now();
    };

    constexpr int warmup_switches = 8;
    for (int index = 0; index < warmup_switches; ++index) {
        const std::uint64_t commits_before = window->CommitCount();
        const std::uint64_t completed_before = window->AsyncCommitStats().completed;
        window->SetTheme(next_theme());
        finish_frame(commits_before, completed_before);
    }

    std::vector<double> apply_times;
    std::vector<double> settle_times;
    std::vector<double> total_times;
    std::vector<double> draw_times;
    std::vector<double> commit_times;
    std::vector<double> present_times;
    apply_times.reserve(static_cast<std::size_t>(switches));
    settle_times.reserve(static_cast<std::size_t>(switches));
    total_times.reserve(static_cast<std::size_t>(switches));
    draw_times.reserve(static_cast<std::size_t>(switches));
    commit_times.reserve(static_cast<std::size_t>(switches));
    present_times.reserve(static_cast<std::size_t>(switches));

    double first_total = 0.0;
    for (int index = 0; index < switches; ++index) {
        const std::uint64_t commits_before = window->CommitCount();
        const std::uint64_t completed_before = window->AsyncCommitStats().completed;

        // SetTheme re-resolves resources, lays out and submits one frame, so the
        // apply span already contains the draw and commit calls.
        const auto start = std::chrono::steady_clock::now();
        window->SetTheme(next_theme());
        const auto after_apply = std::chrono::steady_clock::now();
        const auto after_settle = finish_frame(commits_before, completed_before);

        const auto timings = window->LastFrameTimings();
        const auto latency = window->LastPresentedFrame();
        const double apply_ms =
            std::chrono::duration<double, std::milli>(after_apply - start).count();
        const double settle_ms =
            std::chrono::duration<double, std::milli>(after_settle - after_apply).count();
        apply_times.push_back(apply_ms);
        settle_times.push_back(settle_ms);
        const double total = apply_ms + settle_ms;
        total_times.push_back(total);
        if (index == 0) {
            first_total = total;
        }
        draw_times.push_back(timings.draw_ms);
        commit_times.push_back(timings.commit_ms);
        present_times.push_back(latency.sequence != 0 ? latency.present_ms : 0.0);
    }

    const auto apply_summary = Summarize(std::move(apply_times));
    const auto settle_summary = Summarize(std::move(settle_times));
    const auto total_summary = Summarize(std::move(total_times));
    const auto draw_summary = Summarize(std::move(draw_times));
    const auto commit_summary = Summarize(std::move(commit_times));
    const auto present_summary = Summarize(std::move(present_times));
    const auto memory = ReadMemory();
    const auto stats = window->AsyncCommitStats();

    std::printf(
        "theme switches=%d cycle=%s async=%s render=%s throttle=%s dpi=%.0f "
        "apply=%.3f/%.3f/%.3f settle=%.3f/%.3f/%.3f "
        "total=%.3f/%.3f/%.3f first=%.3f draw=%.3f/%.3f commit=%.3f/%.3f "
        "present=%.3f/%.3f ws=%.1f MB ws_peak=%.1f MB private_peak=%.1f MB "
        "submitted=%llu completed=%llu dropped=%llu\n",
        switches,
        contrast ? "light-dark-contrast" : "light-dark",
        async_commit ? "on" : "off",
        software ? "software" : "hardware",
        throttle ? "on" : "off",
        static_cast<double>(window->DpiScale()) * 96.0,
        apply_summary.average,
        apply_summary.p99,
        apply_summary.max,
        settle_summary.average,
        settle_summary.p99,
        settle_summary.max,
        total_summary.average,
        total_summary.p99,
        total_summary.max,
        first_total,
        draw_summary.average,
        draw_summary.p99,
        commit_summary.average,
        commit_summary.p99,
        present_summary.average,
        present_summary.p99,
        memory.working_set_mb,
        memory.peak_working_set_mb,
        memory.peak_private_mb,
        static_cast<unsigned long long>(stats.submitted),
        static_cast<unsigned long long>(stats.completed),
        static_cast<unsigned long long>(stats.dropped));

    window->Close();
    return 0;
}
