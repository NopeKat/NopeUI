#include <atomic>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>

#include <Windows.h>

#include "form_page.h"
#include "nopeui/platform/window.h"
#include "nopeui/theme/theme.h"

namespace {

std::wstring FindTracePath(std::wstring_view command_line) {
    const std::wstring_view marker = L"--trace";
    const size_t marker_pos = command_line.find(marker);
    if (marker_pos == std::wstring_view::npos) {
        return {};
    }

    size_t pos = marker_pos + marker.size();
    while (pos < command_line.size() && command_line[pos] == L' ') {
        ++pos;
    }
    if (pos >= command_line.size()) {
        return {};
    }
    if (command_line[pos] == L'"') {
        const size_t end = command_line.find(L'"', pos + 1);
        if (end == std::wstring_view::npos) {
            return {};
        }
        return std::wstring(command_line.substr(pos + 1, end - pos - 1));
    }

    const size_t end = command_line.find(L' ', pos);
    return std::wstring(command_line.substr(pos, end == std::wstring_view::npos
                                                   ? std::wstring_view::npos
                                                   : end - pos));
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR command_line_ptr, int) {
    auto result = nopeui::Window::Create(900, 640, u"NopeUI Window Smoke");
    if (!result.ok()) {
        return 1;
    }

    std::wstring_view command_line =
        command_line_ptr != nullptr ? std::wstring_view(command_line_ptr) : std::wstring_view{};
    auto window = std::move(result).value();
    auto* window_ptr = window.Get();

    auto page = nopeui::samples::BuildFormPage([window_ptr](bool dark) {
        window_ptr->SetTheme(dark ? nopeui::DarkTheme() : nopeui::LightTheme());
    });
    window->SetContent(page);
    window->SetStyleSheet(nopeui::samples::BuildFormStyleSheet());

    if (command_line.find(L"--light") != std::wstring_view::npos) {
        window->SetTheme(nopeui::LightTheme());
    }
    if (command_line.find(L"--dark") != std::wstring_view::npos) {
        window->SetTheme(nopeui::DarkTheme());
    }

    window->Show();
    if (command_line.find(L"--sync") != std::wstring_view::npos) {
        window->SetAsyncCommit(false);
        window->RenderFrame();
    }
    if (command_line.find(L"--software") != std::wstring_view::npos) {
        window->SetSoftwareRender(true);
    }

    const auto trace_path = FindTracePath(command_line);
    std::FILE* trace = nullptr;
    if (!trace_path.empty()) {
        std::FILE* opened = nullptr;
        _wfopen_s(&opened, trace_path.c_str(), L"w");
        trace = opened;
        if (trace == nullptr) {
            std::fwprintf(stderr, L"trace open failed: %s\n", trace_path.c_str());
            return 1;
        }
        std::setvbuf(trace, nullptr, _IONBF, 0);
        std::fprintf(
            trace,
            "time_ms,frames,commits,latency_sequence,queue_ms,commit_ms,present_ms,"
            "submitted,completed,dropped,waits,max_depth\n");
    }

    std::atomic<bool> stop_trace{false};
    std::thread trace_thread;
    if (trace != nullptr) {
        trace_thread = std::thread([&] {
            const auto start = std::chrono::steady_clock::now();
            std::uint64_t last_frames = ~0ULL;
            std::uint64_t last_commits = ~0ULL;
            std::uint64_t last_latency_sequence = ~0ULL;
            while (!stop_trace.load(std::memory_order_relaxed)) {
                Sleep(4);
                const auto frames = window->FrameCount();
                const auto commits = window->CommitCount();
                const auto latency = window->LastPresentedFrame();
                const auto stats = window->AsyncCommitStats();
                if (frames == last_frames && commits == last_commits &&
                    latency.sequence == last_latency_sequence) {
                    continue;
                }
                last_frames = frames;
                last_commits = commits;
                last_latency_sequence = latency.sequence;
                const double elapsed_ms =
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start).count();
                std::fprintf(
                    trace,
                    "%.2f,%llu,%llu,%llu,%.3f,%.3f,%.3f,%llu,%llu,%llu,%llu,%u\n",
                    elapsed_ms,
                    static_cast<unsigned long long>(frames),
                    static_cast<unsigned long long>(commits),
                    static_cast<unsigned long long>(latency.sequence),
                    latency.queue_ms,
                    latency.commit_ms,
                    latency.present_ms,
                    static_cast<unsigned long long>(stats.submitted),
                    static_cast<unsigned long long>(stats.completed),
                    static_cast<unsigned long long>(stats.dropped),
                    static_cast<unsigned long long>(stats.waits),
                    stats.max_depth);
            }
        });
    }

    wchar_t title[128]{};
    std::swprintf(
        title,
        std::size(title),
        L"NopeUI Window Smoke - last frame %.2f ms",
        window->LastFrameMilliseconds());
    SetWindowTextW(window->Handle(), title);

    auto loop = nopeui::RunMessageLoop();

    stop_trace.store(true, std::memory_order_relaxed);
    if (trace_thread.joinable()) {
        trace_thread.join();
    }
    if (trace != nullptr) {
        std::fclose(trace);
    }
    if (!loop.ok()) {
        return 1;
    }
    return loop.value();
}
