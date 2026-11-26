#include <cwchar>
#include <iterator>
#include <utility>

#include <Windows.h>

#include "nopeui/platform/window.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR command_line, int) {
    auto result = nopeui::Window::Create(900, 600, u"NopeUI Window Smoke");
    if (!result.ok()) {
        return 1;
    }

    auto window = std::move(result).value();
    window->Show();
    if (command_line != nullptr && std::wcsstr(command_line, L"--sync") != nullptr) {
        window->SetAsyncCommit(false);
        window->RenderFrame();
    }

    wchar_t title[128]{};
    std::swprintf(
        title,
        std::size(title),
        L"NopeUI Window Smoke - last frame %.2f ms",
        window->LastFrameMilliseconds());
    SetWindowTextW(window->Handle(), title);

    auto loop = nopeui::RunMessageLoop();
    if (!loop.ok()) {
        return 1;
    }
    return loop.value();
}
