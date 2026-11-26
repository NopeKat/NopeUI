#include "../test_check.h"
#include <chrono>
#include <utility>

#include <Windows.h>

#include "nopeui/platform/window.h"

namespace {

bool WaitForCommits(nopeui::Window& window, std::uint64_t target) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (window.CommitCount() < target) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        Sleep(1);
    }
    return true;
}

}  // namespace

int main() {
    auto result = nopeui::Window::Create(320, 200, u"NopeUI Async Window Test");
    if (!result.ok()) {
        return 1;
    }

    auto window = std::move(result).value();
    NOPEUI_CHECK(window->IsAsyncCommitEnabled());
    NOPEUI_CHECK(window->FrameCount() >= 1);
    window->SetCommitThrottle(false);

    const auto commits_before = window->CommitCount();
    window->RenderFrame();
    NOPEUI_CHECK(WaitForCommits(*window, commits_before + 1));
    NOPEUI_CHECK(window->LastPresentedFrame().sequence >= 2);
    NOPEUI_CHECK(window->AsyncCommitStats().submitted >= 1);
    NOPEUI_CHECK(window->AsyncCommitStats().completed >= 1);

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
    window->RenderFrame();
    NOPEUI_CHECK(window->FrameCount() > frames_before_resize);
    NOPEUI_CHECK(WaitForCommits(*window, commits_before + 2));

    window->RenderFrame(nopeui::Rect{0.0f, 0.0f, 200.0f, 4.0f});
    NOPEUI_CHECK(WaitForCommits(*window, commits_before + 3));

    window->SetAsyncCommit(false);
    NOPEUI_CHECK(!window->IsAsyncCommitEnabled());
    const auto commits_after_disable = window->CommitCount();
    window->RenderFrame(nopeui::Rect{0.0f, 0.0f, 200.0f, 4.0f});
    NOPEUI_CHECK(window->CommitCount() > commits_after_disable);

    window->Close();
    NOPEUI_CHECK(window->Handle() == nullptr);

    auto loop = nopeui::RunMessageLoop();
    if (!loop.ok()) {
        return 1;
    }
    NOPEUI_CHECK(loop.value() == 0);
    return 0;
}
