#include "../test_check.h"

#include <cmath>
#include <cstdint>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include "nopeui/platform/window.h"

namespace {

void SendDpiChange(nopeui::Window& window, UINT dpi, int width, int height) {
    RECT suggested{0, 0, width, height};
    SendMessageW(
        window.Handle(),
        WM_DPICHANGED,
        MAKEWPARAM(dpi, dpi),
        reinterpret_cast<LPARAM>(&suggested));
}

}  // namespace

int main() {
    auto result = nopeui::Window::Create(320, 200, u"NopeUI DPI Test");
    if (!result.ok()) {
        return 1;
    }

    auto window = std::move(result).value();
    // Commit synchronously so commit counts move inside the same call.
    window->SetAsyncCommit(false);
    window->SetCommitThrottle(false);

    const float created_scale = window->DpiScale();
    NOPEUI_CHECK(created_scale >= 1.0f);

    const std::uint64_t frames_before = window->FrameCount();
    const std::uint64_t commits_before = window->CommitCount();

    // The new DPI travels in wParam; the suggested window rect is in lParam.
    // The target always differs from the current monitor scale, so the
    // assertion cannot pass without reading the message.
    const UINT target_dpi = created_scale >= 1.5f ? 96u : 144u;
    SendDpiChange(*window, target_dpi, 480, 300);

    NOPEUI_CHECK(
        std::abs(window->DpiScale() - static_cast<float>(target_dpi) / 96.0f) < 0.001f);

    RECT resized{};
    NOPEUI_CHECK(GetWindowRect(window->Handle(), &resized) != FALSE);
    NOPEUI_CHECK(std::abs(resized.right - resized.left - 480) <= 2);
    NOPEUI_CHECK(std::abs(resized.bottom - resized.top - 300) <= 2);

    // The surfaces were rebuilt for the new scale, so the repaint has to
    // succeed instead of leaving an empty frame.
    NOPEUI_CHECK(window->FrameCount() > frames_before);
    NOPEUI_CHECK(window->CommitCount() > commits_before);
    NOPEUI_CHECK(window->LastFrameTimings().draw_ms > 0.0);

    // A second change back to the original scale exercises the same reset path
    // twice and must keep committing.
    const std::uint64_t frames_after_first = window->FrameCount();
    const UINT restore_dpi = static_cast<UINT>(std::lround(created_scale * 96.0f));
    SendDpiChange(*window, restore_dpi, 320, 200);
    NOPEUI_CHECK(std::abs(window->DpiScale() - created_scale) < 0.001f);
    NOPEUI_CHECK(window->FrameCount() > frames_after_first);

    const auto commits_after_restore = window->CommitCount();
    window->RenderFrame();
    NOPEUI_CHECK(window->CommitCount() > commits_after_restore);

    window->Close();
    NOPEUI_CHECK(window->Handle() == nullptr);

    auto loop = nopeui::RunMessageLoop();
    if (!loop.ok()) {
        return 1;
    }
    NOPEUI_CHECK(loop.value() == 0);
    return 0;
}
