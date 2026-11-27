#include <Windows.h>

#include <filesystem>
#include <string>
#include <string_view>

#include "nopeui/base/strings.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/flyout.h"
#include "nopeui/controls/label.h"
#include "nopeui/markup/xaml.h"
#include "nopeui/platform/window.h"

namespace {

constexpr std::string_view kPagePath = "page.xaml";

std::wstring ErrorText(const nopeui::Error& error) {
    auto converted = nopeui::Utf8ToUtf16(error.message);
    if (!converted.ok()) {
        return L"XAML load failed";
    }
    return std::wstring(converted.value().begin(), converted.value().end());
}

}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    auto created = nopeui::Window::Create(720, 480, u"NopeUI XAML Smoke");
    if (!created.ok()) {
        return 1;
    }
    auto window = std::move(created).value();
    nopeui::Window* window_ptr = window.Get();

    nopeui::xaml::LoadContext context;
    context.resolve_event = [window_ptr](std::wstring_view name) {
        const std::wstring handler_name(name);
        return [window_ptr, handler_name](const nopeui::xaml::EventArgs& args) {
            if (args.event == L"Click" && handler_name == L"ShowFlyout" &&
                args.sender != nullptr) {
                auto content = nopeui::Label::Create(L"Flyout content from XAML");
                auto flyout = nopeui::Flyout::Create();
                flyout->SetContent(content);
                const auto bounds = args.sender->Bounds();
                window_ptr->ShowPopup(
                    flyout,
                    nopeui::Point{bounds.x, bounds.Bottom() + 6.0f});
            }
        };
    };

    auto loaded = [&] {
        wchar_t module_path[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, module_path, MAX_PATH) == 0) {
            return nopeui::Result<nopeui::xaml::LoadResult>::Fail(
                nopeui::Error{nopeui::ErrorCode::NotFound, "module path unavailable"});
        }
        const std::filesystem::path page_path =
            std::filesystem::path(module_path).parent_path() /
            std::string(kPagePath);
        const auto utf8 = page_path.u8string();
        return nopeui::xaml::LoadFile(
            std::string(utf8.begin(), utf8.end()), context);
    }();
    if (!loaded.ok()) {
        MessageBoxW(
            nullptr,
            ErrorText(loaded.error()).c_str(),
            L"NopeUI XAML Smoke",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    auto page = std::move(loaded).value();
    window->SetContent(page.root);
    window->Show();
    auto loop = nopeui::RunMessageLoop();
    return loop.ok() ? loop.value() : 1;
}
