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

    window->Close();
    NOPEUI_CHECK(window->Handle() == nullptr);

    auto loop = nopeui::RunMessageLoop();
    if (!loop.ok()) {
        return 1;
    }
    NOPEUI_CHECK(loop.value() == 0);

    return 0;
}
