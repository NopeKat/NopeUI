#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include "nopeui/platform/window.h"

int main(int argc, char** argv) {
    std::size_t row_count = 100000;
    double scroll_step = 4.0;
    bool cache_layouts = false;
    int width = 900;
    int height = 600;
    int surfaces = 0;
    bool exact_client = false;
    bool async_commit = true;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--rows") == 0 && index + 1 < argc) {
            row_count = static_cast<std::size_t>(std::strtoull(argv[++index], nullptr, 10));
        } else if (std::strcmp(argv[index], "--scroll") == 0 && index + 1 < argc) {
            scroll_step = std::strtod(argv[++index], nullptr);
        } else if (std::strcmp(argv[index], "--cache") == 0) {
            cache_layouts = true;
        } else if (std::strcmp(argv[index], "--width") == 0 && index + 1 < argc) {
            width = std::atoi(argv[++index]);
        } else if (std::strcmp(argv[index], "--height") == 0 && index + 1 < argc) {
            height = std::atoi(argv[++index]);
        } else if (std::strcmp(argv[index], "--surfaces") == 0 && index + 1 < argc) {
            surfaces = std::atoi(argv[++index]);
        } else if (std::strcmp(argv[index], "--client") == 0) {
            exact_client = true;
        } else if (std::strcmp(argv[index], "--sync") == 0) {
            async_commit = false;
        }
    }
    if (row_count == 0) {
        row_count = 1;
    }
    if (width <= 0 || height <= 0) {
        width = 900;
        height = 600;
    }

    std::vector<std::wstring> rows;
    rows.reserve(row_count);
    for (std::size_t index = 0; index < row_count; ++index) {
        wchar_t buffer[96]{};
        std::swprintf(
            buffer,
            std::size(buffer),
            L"Item %06zu  NopeUI virtual text list sample",
            index);
        rows.emplace_back(buffer);
    }

    auto result = nopeui::Window::Create(width, height, u"NopeUI Text List Benchmark");
    if (!result.ok()) {
        std::fprintf(stderr, "Window::Create failed: %s\n", result.error().message.c_str());
        return 1;
    }

    auto window = std::move(result).value();
    window->Show();
    if (exact_client) {
        RECT frame{0, 0, width, height};
        const DWORD style = static_cast<DWORD>(
            GetWindowLongPtrW(window->Handle(), GWL_STYLE));
        AdjustWindowRectExForDpi(
            &frame, style, FALSE, 0, GetDpiForWindow(window->Handle()));
        SetWindowPos(
            window->Handle(),
            nullptr,
            0,
            0,
            frame.right - frame.left,
            frame.bottom - frame.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateWindow(window->Handle());
        window->RenderFrame();
    }
    window->SetDiagnosticAsyncSurfaces(surfaces);
    window->SetAsyncCommit(async_commit);
    window->SetDiagnosticTextList(rows, 32.0f, cache_layouts);

    RECT client{};
    GetClientRect(window->Handle(), &client);
    const int client_width = client.right - client.left;
    const int client_height = client.bottom - client.top;
    const int awareness = static_cast<int>(
        GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext()));
    const int dpi = static_cast<int>(GetDpiForWindow(window->Handle()));
    std::vector<double> frame_times;
    std::vector<double> begin_times;
    std::vector<double> draw_times;
    std::vector<double> end_times;
    std::vector<double> commit_times;
    std::vector<double> queue_times;
    std::vector<double> present_times;
    constexpr int warmup_frames = 30;
    constexpr int measured_frames = 300;
    frame_times.reserve(measured_frames);
    begin_times.reserve(measured_frames);
    draw_times.reserve(measured_frames);
    end_times.reserve(measured_frames);
    commit_times.reserve(measured_frames);
    queue_times.reserve(measured_frames);
    present_times.reserve(measured_frames);

    double scroll = 0.0;
    for (int index = 0; index < warmup_frames + measured_frames; ++index) {
        scroll += scroll_step;
        window->SetDiagnosticScroll(static_cast<float>(scroll));
        const auto start = std::chrono::steady_clock::now();
        window->RenderFrame();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (index >= warmup_frames) {
            const auto timings = window->LastFrameTimings();
            frame_times.push_back(
                std::chrono::duration<double, std::milli>(elapsed).count());
            begin_times.push_back(timings.begin_draw_ms);
            draw_times.push_back(timings.draw_ms);
            end_times.push_back(timings.end_draw_ms);
            const auto latency = window->LastPresentedFrame();
            commit_times.push_back(latency.commit_ms);
            queue_times.push_back(latency.queue_ms);
            present_times.push_back(latency.present_ms);
        }
        Sleep(8);
    }

    const auto summarize = [](std::vector<double> values) {
        std::sort(values.begin(), values.end());
        double total = 0.0;
        for (const double value : values) {
            total += value;
        }
        const double average = total / static_cast<double>(values.size());
        const double p99 = values[static_cast<std::size_t>(
            static_cast<double>(values.size() - 1) * 0.99)];
        return std::pair{average, p99};
    };
    const auto [frame_average, frame_p99] = summarize(std::move(frame_times));
    const auto [begin_average, begin_p99] = summarize(std::move(begin_times));
    const auto [draw_average, draw_p99] = summarize(std::move(draw_times));
    const auto [end_average, end_p99] = summarize(std::move(end_times));
    const auto [commit_average, commit_p99] = summarize(std::move(commit_times));
    const auto [queue_average, queue_p99] = summarize(std::move(queue_times));
    const auto [present_average, present_p99] = summarize(std::move(present_times));

    std::printf(
        "text_list requested=%dx%d client=%dx%d dpi=%d awareness=%d surfaces=%d async=%s rows=%zu cache=%s scroll=%.1f "
        "avg=%.3f ms p99=%.3f ms begin=%.3f/%.3f draw=%.3f/%.3f end=%.3f/%.3f "
        "commit=%.3f/%.3f queue=%.3f/%.3f present=%.3f/%.3f\n",
        width,
        height,
        client_width,
        client_height,
        dpi,
        awareness,
        window->DiagnosticAsyncSurfaceCount(),
        async_commit ? "on" : "off",
        row_count,
        cache_layouts ? "on" : "off",
        scroll_step,
        frame_average,
        frame_p99,
        begin_average,
        begin_p99,
        draw_average,
        draw_p99,
        end_average,
        end_p99,
        commit_average,
        commit_p99,
        queue_average,
        queue_p99,
        present_average,
        present_p99);

    window->Close();
    return 0;
}
