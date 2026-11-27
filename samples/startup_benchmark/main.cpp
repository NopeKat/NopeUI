#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

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

    Clock clock;
    const std::int64_t process_start = clock.ProcessStartTicks();
    const std::int64_t entry = clock.Now();

    bool content_form = true;
    bool async_commit = true;
    bool software = false;
    const char* theme_name = "system";
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--no-content") == 0) {
            content_form = false;
        } else if (std::strcmp(argv[index], "--sync") == 0) {
            async_commit = false;
        } else if (std::strcmp(argv[index], "--software") == 0) {
            software = true;
        } else if (std::strcmp(argv[index], "--theme") == 0 && index + 1 < argc) {
            theme_name = argv[++index];
        }
    }

    auto created = nopeui::Window::Create(900, 640, u"NopeUI Startup Benchmark");
    if (!created.ok()) {
        std::fprintf(stderr, "Window::Create failed: %s\n", created.error().message.c_str());
        return 1;
    }
    const std::int64_t after_create = clock.Now();

    auto window = std::move(created).value();
    window->SetAsyncCommit(async_commit);
    if (software) {
        window->SetSoftwareRender(true);
    }
    if (std::strcmp(theme_name, "light") == 0) {
        window->SetTheme(nopeui::LightTheme());
    } else if (std::strcmp(theme_name, "dark") == 0) {
        window->SetTheme(nopeui::DarkTheme());
    } else if (std::strcmp(theme_name, "contrast") == 0) {
        window->SetTheme(nopeui::HighContrastTheme());
    }
    if (content_form) {
        window->SetContent(BuildFormPage());
        window->SetStyleSheet(BuildFormStyleSheet());
    }
    const std::int64_t after_content = clock.Now();

    window->Show();
    // Show already submits the first frame; this only waits for the commit
    // queue, so the total stays process start to first finished frame.
    const std::int64_t after_show = clock.Now();

    if (!WaitForFrame(*window, 5000)) {
        std::fprintf(stderr, "no committed frame within 5000 ms\n");
        window->Close();
        return 1;
    }
    const std::int64_t after_frame = clock.Now();
    const auto first_memory = ReadMemory();

    // The presentation estimate comes from DWM timing, so give it a short
    // window before reporting; the frame itself is already committed.
    const auto present_deadline = std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(100);
    while (window->LastPresentedFrame().sequence == 0 &&
           std::chrono::steady_clock::now() < present_deadline) {
        SwitchToThread();
    }
    const std::int64_t after_present = clock.Now();

    const auto final_memory = ReadMemory();
    const auto timings = window->LastFrameTimings();
    const auto latency = window->LastPresentedFrame();

    std::printf(
        "startup content=%s theme=%s render=%s async=%s size=900x640 dpi=%.0f "
        "total=%.1f ms submit=%.1f entry=%.1f create=%.1f content=%.1f show=%.1f "
        "settle=%.1f present_wait=%.1f "
        "draw=%.1f begin=%.1f end=%.1f commit=%.1f present=%.1f "
        "commits=%llu frames=%llu "
        "ws=%.1f MB ws_peak=%.1f MB private_peak=%.1f MB "
        "ws_peak_first=%.1f MB\n",
        content_form ? "form" : "placeholder",
        theme_name,
        software ? "software" : "hardware",
        async_commit ? "on" : "off",
        static_cast<double>(window->DpiScale()) * 96.0,
        clock.Milliseconds(after_frame - process_start),
        clock.Milliseconds(after_show - process_start),
        clock.Milliseconds(entry - process_start),
        clock.Milliseconds(after_create - entry),
        clock.Milliseconds(after_content - after_create),
        clock.Milliseconds(after_show - after_content),
        clock.Milliseconds(after_frame - after_show),
        clock.Milliseconds(after_present - after_frame),
        timings.draw_ms,
        timings.begin_draw_ms,
        timings.end_draw_ms,
        timings.commit_ms,
        latency.present_ms,
        static_cast<unsigned long long>(window->CommitCount()),
        static_cast<unsigned long long>(window->FrameCount()),
        final_memory.working_set_mb,
        final_memory.peak_working_set_mb,
        final_memory.peak_private_mb,
        first_memory.peak_working_set_mb);

    window->Close();
    return 0;
}
