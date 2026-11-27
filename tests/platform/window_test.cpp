#include "../test_check.h"
#include <cmath>
#include <utility>

#include <Windows.h>

#include "nopeui/platform/window.h"

int main() {
    auto result = nopeui::Window::Create(320, 200, u"NopeUI Window Test");
    if (!result.ok()) {
        return 1;
    }

    auto window = std::move(result).value();

    // Create sizes the native window from logical DIPs at the monitor scale.
    const float dpi_scale = window->DpiScale();
    NOPEUI_CHECK(dpi_scale >= 1.0f);
    RECT created_rect{};
    NOPEUI_CHECK(GetWindowRect(window->Handle(), &created_rect) != FALSE);
    const int created_width = static_cast<int>(created_rect.right - created_rect.left);
    NOPEUI_CHECK(
        std::abs(created_width -
                 static_cast<int>(std::lround(320.0f * dpi_scale))) <= 2);

    NOPEUI_CHECK(window->IsAsyncCommitEnabled());
    window->SetAsyncCommit(false);
    NOPEUI_CHECK(static_cast<bool>(window));
    NOPEUI_CHECK(window->Handle() != nullptr);
    NOPEUI_CHECK(IsWindow(window->Handle()) != FALSE);
    NOPEUI_CHECK(!window->IsVisible());
    NOPEUI_CHECK(window->FrameCount() >= 1);
    NOPEUI_CHECK(window->LastFrameMilliseconds() >= 0.0);

    const auto frames_before_resize = window->FrameCount();
    NOPEUI_CHECK(
        SetWindowPos(
            window->Handle(),
            nullptr,
            0,
            0,
            480,
            320,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE);
    NOPEUI_CHECK(window->FrameCount() == frames_before_resize);
    window->RenderFrame();
    NOPEUI_CHECK(window->FrameCount() > frames_before_resize);
    NOPEUI_CHECK(window->CommitCount() >= 2);
    window->RenderFrame(nopeui::Rect{0.0f, 0.0f, 200.0f, 4.0f});
    NOPEUI_CHECK(window->FrameCount() >= 3);

    window->Present();
    const auto commits_before = window->CommitCount();
    window->SetCommitThrottle(false);
    window->RenderFrame(nopeui::Rect{0.0f, 0.0f, 200.0f, 4.0f});
    NOPEUI_CHECK(window->CommitCount() > commits_before);

    window->Close();
    NOPEUI_CHECK(window->Handle() == nullptr);

    auto loop = nopeui::RunMessageLoop();
    if (!loop.ok()) {
        return 1;
    }
    NOPEUI_CHECK(loop.value() == 0);

    return 0;
}
