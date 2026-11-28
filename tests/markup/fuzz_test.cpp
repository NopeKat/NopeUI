// Fuzz regression test. It covers the four claims behind the 24 hour run:
// mangled XAML reaches Parse and Load without taking the process down, the
// malformed corpus stays bounded, deep nesting and resource/template/trigger
// shapes do not crash, and a run leaves a log plus an exportable package as
// evidence.

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "nopeui/base/diagnostic.h"
#include "nopeui/markup/xaml.h"

#include "../test_check.h"

namespace {

namespace fs = std::filesystem;

std::wstring TempDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    ::GetTempPathW(MAX_PATH, buffer);
    const std::wstring root(buffer);
    const std::wstring path =
        root + L"NopeUI-fuzz-test-" + std::to_wstring(::GetCurrentProcessId());
    ::CreateDirectoryW(path.c_str(), nullptr);
    return path;
}

std::string Utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = static_cast<int>(text.size());
    const int bytes = ::WideCharToMultiByte(
        CP_UTF8, 0, text.c_str(), length, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return {};
    }
    std::string out(static_cast<std::size_t>(bytes), '\0');
    ::WideCharToMultiByte(
        CP_UTF8, 0, text.c_str(), length, out.data(), bytes, nullptr, nullptr);
    return out;
}

class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed == 0 ? 0x9e3779b97f4a7c15ULL : seed) {}

    std::uint64_t Next() {
        std::uint64_t value = state_;
        value ^= value << 13;
        value ^= value >> 7;
        value ^= value << 17;
        state_ = value;
        return value;
    }

    std::size_t Below(std::size_t bound) {
        return bound == 0 ? 0 : static_cast<std::size_t>(Next() % bound);
    }

private:
    std::uint64_t state_;
};

const std::wstring_view kAlphabet = L"<>/=\"'& \n\t:{}[],._-+*=()0123456789abcXYZ#%";

const wchar_t* const kFragments[] = {
    L"<", L">", L"/>", L"</", L"<Grid>", L"</Grid>", L"<Label ", L"Text=\"",
    L"x:Name=\"", L"&amp;", L"&#x41;", L"<!--", L"-->", L"Width=\"-",
    L"Margin=\"1,2", L"<StackPanel.Resources>", L"<Trigger Property=\"IsEnabled\">",
};

std::wstring Mutate(const std::wstring& seed, Rng& rng) {
    std::wstring text = seed;
    const std::size_t operations = 1 + rng.Below(4);
    for (std::size_t step = 0; step < operations; ++step) {
        const std::size_t size = text.size();
        switch (rng.Below(6)) {
        case 0:
            text.insert(text.begin() + static_cast<std::ptrdiff_t>(rng.Below(size + 1)),
                kAlphabet[rng.Below(kAlphabet.size())]);
            break;
        case 1:
            if (size != 0) {
                const std::size_t at = rng.Below(size);
                text.erase(at, 1 + rng.Below(size - at));
            }
            break;
        case 2:
            if (size != 0) {
                text[rng.Below(size)] = kAlphabet[rng.Below(kAlphabet.size())];
            }
            break;
        case 3: {
            const std::wstring_view fragment = kFragments[rng.Below(std::size(kFragments))];
            text.insert(rng.Below(size + 1), fragment);
            break;
        }
        case 4:
            text.resize(rng.Below(size + 1));
            break;
        default:
            if (size > 1) {
                std::swap(text[rng.Below(size)], text[rng.Below(size)]);
            }
            break;
        }
    }
    return text;
}

std::vector<std::wstring> Corpus() {
    std::vector<std::wstring> seeds;
    seeds.push_back(L"<StackPanel xmlns=\"https://nopeui.local/xaml\"><Label Text=\"Hi\"/></StackPanel>");
    seeds.push_back(
        L"<StackPanel><StackPanel.Resources>"
        L"<Style x:Key=\"s\" TargetType=\"Button\"><Setter Property=\"CornerRadius\" Value=\"6\"/>"
        L"</Style></StackPanel.Resources><Button x:Key=\"s\" Text=\"A &amp; B\"/></StackPanel>");
    seeds.push_back(L"<Border Width=\"-1\" Margin=\"1,2,3\" CornerRadius=\"x\" Background=\"\"/>");
    seeds.push_back(L"<Grid><Grid.RowDefinitions><RowDefinition/></Grid.RowDefinitions></Grid>");
    seeds.push_back(L"<StackPanel><Label Text=\"&#x41;&#65;&amp;&amp;&amp;\"/></StackPanel>");
    seeds.push_back(L"<StackPanel><Label Text=\"");  // truncated on purpose
    seeds.push_back(L"<");
    seeds.push_back(L"");

    // Deep nesting exercises the recursive descent path that a short run rarely
    // reaches.
    std::wstring deep;
    for (int index = 0; index < 200; ++index) {
        deep += L"<StackPanel>";
    }
    for (int index = 0; index < 200; ++index) {
        deep += L"</StackPanel>";
    }
    seeds.push_back(std::move(deep));

    // Template and trigger shapes the loader sees but does not implement yet.
    seeds.push_back(
        L"<StackPanel><StackPanel.Resources><ControlTemplate x:Key=\"t\">"
        L"<Border><ContentPresenter/></Border></ControlTemplate></StackPanel.Resources>"
        L"<Button><Button.Triggers><Trigger Property=\"IsEnabled\" Value=\"False\">"
        L"<Setter Property=\"Opacity\" Value=\"0.5\"/></Trigger></Button.Triggers></Button></StackPanel>");
    return seeds;
}

}  // namespace

int main() {
    const std::wstring temp = TempDirectory();
    nopeui::DiagnosticOptions options;
    options.directory = Utf8(temp);
    options.max_file_bytes = 256u * 1024u;
    options.max_files = 2;
    options.detect_hangs = false;
    options.resource_interval_seconds = 0;
    NOPEUI_CHECK(nopeui::StartDiagnostics(options).ok());

    const std::vector<std::wstring> seeds = Corpus();
    for (const std::wstring& seed : seeds) {
        nopeui::xaml::Parse(seed);
        nopeui::xaml::Load(seed);
    }
    nopeui::LogMessage(nopeui::LogLevel::Info, "corpus: seeds survived parse and load");

    Rng rng(0x5eed);
    std::uint64_t iterations = 0;
    std::uint64_t parsed = 0;
    std::uint64_t loaded = 0;
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && iterations < 250000) {
        const std::wstring input = Mutate(seeds[rng.Below(seeds.size())], rng);
        auto parsed_result = nopeui::xaml::Parse(input);
        if (parsed_result.ok()) {
            ++parsed;
            if (nopeui::xaml::Load(input).ok()) {
                ++loaded;
            }
        }
        ++iterations;
    }

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    char line[128] = {};
    std::snprintf(line, sizeof(line), "fuzz: iterations=%llu in %.1fs", 
        static_cast<unsigned long long>(iterations), seconds);
    nopeui::LogMessage(nopeui::LogLevel::Info, line);

    // A 5 second budget must produce far more than a handful of documents; a
    // stall here means the reader stopped making progress.
    NOPEUI_CHECK(iterations >= 20000);
    NOPEUI_CHECK(parsed > 0);

    const std::string directory = nopeui::DiagnosticsDirectory();
    NOPEUI_CHECK(!directory.empty());
    const fs::path log_path = fs::path(directory) / "diagnostics.log";
    NOPEUI_CHECK(fs::exists(log_path));
    NOPEUI_CHECK(fs::file_size(log_path) > 0);

    const std::string environment = nopeui::EnvironmentSummary();
    NOPEUI_CHECK(!environment.empty());

    auto package = nopeui::ExportDiagnostics("fuzz test");
    NOPEUI_CHECK(package.ok());
    NOPEUI_CHECK(fs::exists(fs::path(package.value())));
    NOPEUI_CHECK(fs::file_size(fs::path(package.value())) > 0);

    auto dump = nopeui::WriteDump("fuzz test");
    NOPEUI_CHECK(dump.ok());
    NOPEUI_CHECK(fs::exists(fs::path(dump.value())));
    NOPEUI_CHECK(fs::file_size(fs::path(dump.value())) > 0);

    nopeui::StopDiagnostics();
    NOPEUI_CHECK(nopeui::DiagnosticsDirectory().empty());

    std::error_code ignored;
    fs::remove_all(fs::path(temp), ignored);
    return 0;
}
