#include "../test_check.h"

#include <Windows.h>

#include "nopeui/controls/border.h"
#include "nopeui/controls/context_menu.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/controls/tooltip.h"
#include "nopeui/platform/window.h"

int main() {
    auto created = nopeui::Window::Create(320, 200, u"NopeUI Popup Test");
    if (!created.ok()) {
        return 1;
    }
    auto window = std::move(created).value();
    auto panel = nopeui::StackPanel::Create(4.0f);
    panel->Add(nopeui::Label::Create(L"Popup host"));
    window->SetContent(panel);

    auto tip = nopeui::ToolTip::Create(L"Anchored tip");
    window->ShowPopup(tip, nopeui::Point{20.0f, 20.0f});
    NOPEUI_CHECK(window->HasPopup());
    window->RenderFrame();
    window->ClosePopup();

    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    NOPEUI_CHECK(!window->HasPopup());

    window->ShowPopup(nopeui::ToolTip::Create(L"Second tip"), nopeui::Point{30.0f, 30.0f});
    NOPEUI_CHECK(window->HasPopup());
    window->Close();
    NOPEUI_CHECK(window->Handle() == nullptr);

    auto host = nopeui::Border::Create();
    auto menu = nopeui::ContextMenu::Create();
    host->SetContextMenu(menu);
    NOPEUI_CHECK(host->GetContextMenu() == menu.Get());
    host->SetContextMenu(nopeui::Ref<nopeui::Element>());
    NOPEUI_CHECK(host->GetContextMenu() == nullptr);

    auto label = nopeui::Label::Create(L"Tooltip owner");
    nopeui::ToolTipService::SetToolTip(label.Get(), L"Owner tip");
    NOPEUI_CHECK(nopeui::ToolTipService::GetToolTip(label.Get()) == L"Owner tip");
    nopeui::ToolTipService::ClearToolTip(label.Get());
    NOPEUI_CHECK(nopeui::ToolTipService::GetToolTip(label.Get()).empty());

    return 0;
}
