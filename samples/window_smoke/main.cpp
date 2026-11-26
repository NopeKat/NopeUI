#include <utility>

#include <Windows.h>

#include "nopeui/platform/window.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    auto result = nopeui::Window::Create(900, 600, u"NopeUI Window Smoke");
    if (!result.ok()) {
        return 1;
    }

    auto window = std::move(result).value();
    window->Show();

    auto loop = nopeui::RunMessageLoop();
    if (!loop.ok()) {
        return 1;
    }
    return loop.value();
}
