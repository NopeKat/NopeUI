#ifndef NOPEUI_SAMPLES_PERF_METRICS_H_
#define NOPEUI_SAMPLES_PERF_METRICS_H_

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Psapi.h>
#include <Windows.h>

#include "nopeui/platform/window.h"

namespace nopeui::samples {

// Shared measurement helpers for the startup, theme and preview records, so
// every benchmark reports milliseconds, megabytes and percentiles the same way.

inline std::int64_t FileTimeTicks(const FILETIME& value) noexcept {
    return (static_cast<std::int64_t>(value.dwHighDateTime) << 32) |
           static_cast<std::int64_t>(value.dwLowDateTime);
}

struct MemorySnapshot {
    double working_set_mb = 0.0;
    double peak_working_set_mb = 0.0;
    double peak_private_mb = 0.0;
};

inline MemorySnapshot ReadMemory() noexcept {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    MemorySnapshot snapshot;
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            static_cast<DWORD>(sizeof(counters))) == 0) {
        return snapshot;
    }
    constexpr double megabyte = 1024.0 * 1024.0;
    snapshot.working_set_mb = static_cast<double>(counters.WorkingSetSize) / megabyte;
    snapshot.peak_working_set_mb =
        static_cast<double>(counters.PeakWorkingSetSize) / megabyte;
    snapshot.peak_private_mb =
        static_cast<double>(counters.PeakPagefileUsage) / megabyte;
    return snapshot;
}

class Clock {
public:
    Clock() noexcept {
        QueryPerformanceFrequency(&frequency_);
    }

    std::int64_t Now() const noexcept {
        LARGE_INTEGER value{};
        QueryPerformanceCounter(&value);
        return value.QuadPart;
    }

    double Milliseconds(std::int64_t ticks) const noexcept {
        return static_cast<double>(ticks) * 1000.0 /
               static_cast<double>(frequency_.QuadPart);
    }

    // QPC tick that matches the kernel-reported process creation time, so the
    // loader and CRT startup are part of the measured total.
    std::int64_t ProcessStartTicks() const noexcept {
        FILETIME creation{};
        FILETIME exit{};
        FILETIME kernel{};
        FILETIME user{};
        if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user) == 0) {
            return Now();
        }
        FILETIME wall{};
        GetSystemTimePreciseAsFileTime(&wall);
        const std::int64_t wall_ticks = Now();
        const std::int64_t age_ticks = FileTimeTicks(wall) - FileTimeTicks(creation);
        if (age_ticks <= 0) {
            return wall_ticks;
        }
        return wall_ticks - age_ticks * frequency_.QuadPart / 10000000LL;
    }

private:
    LARGE_INTEGER frequency_{};
};

struct Summary {
    double average = 0.0;
    double p99 = 0.0;
    double max = 0.0;
};

inline Summary Summarize(std::vector<double> values) {
    Summary summary;
    if (values.empty()) {
        return summary;
    }
    std::sort(values.begin(), values.end());
    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    summary.average = total / static_cast<double>(values.size());
    const auto index = static_cast<std::size_t>(
        static_cast<double>(values.size() - 1) * 0.99);
    summary.p99 = values[index];
    summary.max = values.back();
    return summary;
}

// Spins until the first frame is submitted and, when async commit is on,
// finished by the worker. Explicit RenderFrame calls are the only paint path
// in these benchmarks, so the wait is bounded by the commit queue.
inline bool WaitForFrame(Window& window, int timeout_ms) noexcept {
    const auto start = std::chrono::steady_clock::now();
    const auto limit = std::chrono::milliseconds(timeout_ms);
    const bool async = window.IsAsyncCommitEnabled();
    while (std::chrono::steady_clock::now() - start < limit) {
        if (window.CommitCount() > 0) {
            const auto stats = window.AsyncCommitStats();
            if (!async || stats.completed > 0) {
                return true;
            }
        }
        SwitchToThread();
    }
    return false;
}

}  // namespace nopeui::samples

#endif  // NOPEUI_SAMPLES_PERF_METRICS_H_
