#include "../test_check.h"
#include <utility>

#include <Windows.h>

#include "nopeui/platform/window.h"

int main() {
    auto result = nopeui::Window::Create(320, 200, u"NopeUI Window Test");
    if (!result.ok()) {
        return 1;
    }

    auto window = std::move(result).value();
    NOPEUI_CHECK(static_cast<bool>(window));
    NOPEUI_CHECK(window->Handle() != nullptr);
    NOPEUI_CHECK(IsWindow(window->Handle()) != FALSE);
    NOPEUI_CHECK(!window->IsVisible());
    NOPEUI_CHECK(window->FrameCount() >= 1);
    NOPEUI_CHECK(window->LastFrameMilliseconds() >= 0.0);

    NOPEUI_CHECK(
        SetWindowPos(
            window->Handle(),
            nullptr,
            0,
            0,
            480,
            320,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE);
    NOPEUI_CHECK(window->FrameCount() >= 2);
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
