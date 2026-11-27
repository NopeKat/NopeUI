// Mutation fuzzer for the XAML reader. It feeds mangled documents to Parse and,
// when the document survives parsing, to Load, and keeps going until the time
// budget runs out. Any access violation is written to the crash log together
// with the exact input that caused it.

#include <windows.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nopeui/markup/xaml.h"

namespace {

const std::wstring* g_current_input = nullptr;
std::string g_crash_path = "fuzz-crash.xaml";

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

LONG WINAPI OnFatalException(EXCEPTION_POINTERS* pointers) {
    const DWORD code = pointers != nullptr && pointers->ExceptionRecord != nullptr
        ? pointers->ExceptionRecord->ExceptionCode
        : 0;
    if (g_current_input != nullptr) {
        std::ofstream out(g_crash_path, std::ios::binary | std::ios::trunc);
        if (out) {
            out << "exception 0x" << std::hex << code << "\n";
            out << Utf8(*g_current_input);
            out << "\n";
        }
        std::fprintf(stderr, "crash 0x%08lx, input saved to %s\n",
            static_cast<unsigned long>(code), g_crash_path.c_str());
    }
    ::TerminateProcess(::GetCurrentProcess(), 3);
    return EXCEPTION_EXECUTE_HANDLER;
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

const std::wstring_view kAlphabet =
    L"<>/=\"'& \n\t\r:{}[],._-+*=()0123456789"
    L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ#%;";

const wchar_t* const kFragments[] = {
    L"<", L">", L"/>", L"</", L"<Grid>", L"</Grid>", L"<Label ",
    L"Text=\"", L"x:Name=\"", L"&amp;", L"&#x41;", L"<!--", L"-->",
    L"<Button Click=\"", L"Width=\"-", L"Margin=\"1,2", L"<StackPanel.Resources>",
};

wchar_t RandomChar(Rng& rng) {
    return kAlphabet[rng.Below(kAlphabet.size())];
}

std::wstring Mutate(const std::wstring& seed, Rng& rng) {
    std::wstring text = seed;
    const std::size_t operations = 1 + rng.Below(4);
    for (std::size_t step = 0; step < operations; ++step) {
        const std::size_t size = text.size();
        switch (rng.Below(7)) {
        case 0: {
            const std::size_t at = rng.Below(size + 1);
            text.insert(text.begin() + static_cast<std::ptrdiff_t>(at), RandomChar(rng));
            break;
        }
        case 1: {
            if (size == 0) {
                text.push_back(RandomChar(rng));
                break;
            }
            const std::size_t at = rng.Below(size);
            const std::size_t length = 1 + rng.Below(size - at);
            text.erase(at, length);
            break;
        }
        case 2: {
            if (size != 0) {
                text[rng.Below(size)] = RandomChar(rng);
            }
            break;
        }
        case 3: {
            if (size == 0) {
                text.push_back(RandomChar(rng));
                break;
            }
            const std::size_t at = rng.Below(size);
            const std::size_t length = 1 + rng.Below(size - at);
            text.insert(at, text.substr(at, length));
            break;
        }
        case 4: {
            if (size > 1) {
                text.resize(rng.Below(size));
            }
            break;
        }
        case 5: {
            const std::wstring_view fragment =
                kFragments[rng.Below(std::size(kFragments))];
            const std::size_t at = rng.Below(size + 1);
            text.insert(at, fragment);
            break;
        }
        default: {
            if (size >= 2) {
                const std::size_t first = rng.Below(size);
                const std::size_t second = rng.Below(size);
                std::swap(text[first], text[second]);
            }
            break;
        }
        }
    }
    return text;
}

std::vector<std::wstring> SeedCorpus() {
    std::vector<std::wstring> seeds;
    seeds.push_back(L"<StackPanel xmlns=\"https://nopeui.local/xaml\"><Label Text=\"Hi\"/></StackPanel>");
    seeds.push_back(L"<Grid><Grid.RowDefinitions><RowDefinition/></Grid.RowDefinitions></Grid>");
    seeds.push_back(
        L"<StackPanel><StackPanel.Resources>"
        L"<Style x:Key=\"s\" TargetType=\"Button\"><Setter Property=\"CornerRadius\" Value=\"6\"/>"
        L"</Style></StackPanel.Resources><Button x:Key=\"s\" Text=\"A &amp; B\"/></StackPanel>");
    seeds.push_back(L"<?xml version=\"1.0\"?><!-- note --><Border Background=\"#FF0000\" CornerRadius=\"4\"/>");
    seeds.push_back(L"<TextBox Placeholder=\"Type\" Text=\"abc\"/>");
    seeds.push_back(L"<ContextMenu><ContextMenu.Items><Item Text=\"Copy\"/></ContextMenu.Items></ContextMenu>");
    seeds.push_back(L"<");
    seeds.push_back(L"");
    return seeds;
}

bool LoadSeedFiles(const std::string& directory, std::vector<std::wstring>* seeds) {
    namespace fs = std::filesystem;
    fs::path root(directory);
    if (!fs::is_directory(root)) {
        return false;
    }
    std::size_t added = 0;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension().string() != ".xaml") {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        if (!in) {
            continue;
        }
        std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const int length = static_cast<int>(bytes.size());
        std::wstring wide;
        if (length > 0) {
            wide.resize(static_cast<std::size_t>(length));
            if (::MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), length, wide.data(), length) > 0) {
                seeds->push_back(std::move(wide));
                ++added;
            }
        }
    }
    std::printf("seed files: %zu from %s\n", added, directory.c_str());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    double seconds = 60.0;
    double max_iterations = 0.0;
    std::uint64_t seed = 1;
    std::string seed_dir;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--seconds" && index + 1 < argc) {
            seconds = std::atof(argv[++index]);
        } else if (argument == "--iterations" && index + 1 < argc) {
            max_iterations = std::atof(argv[++index]);
        } else if (argument == "--seed" && index + 1 < argc) {
            seed = static_cast<std::uint64_t>(std::strtoull(argv[++index], nullptr, 10));
        } else if (argument == "--seed-dir" && index + 1 < argc) {
            seed_dir = argv[++index];
        } else if (argument == "--crash" && index + 1 < argc) {
            g_crash_path = argv[++index];
        } else {
            std::printf("unknown argument: %s\n", argv[index]);
            return 2;
        }
    }

    ::SetUnhandledExceptionFilter(OnFatalException);

    std::vector<std::wstring> seeds = SeedCorpus();
    if (!seed_dir.empty() && !LoadSeedFiles(seed_dir, &seeds)) {
        std::printf("seed directory not found: %s\n", seed_dir.c_str());
        return 2;
    }

    Rng rng(seed);
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::duration<double>(seconds);
    auto next_report = start + std::chrono::seconds(5);

    std::uint64_t iterations = 0;
    std::uint64_t parsed = 0;
    std::uint64_t loaded = 0;
    std::wstring input;

    while (std::chrono::steady_clock::now() < deadline) {
        const std::wstring& base = seeds[rng.Below(seeds.size())];
        input = Mutate(base, rng);
        g_current_input = &input;

        auto parsed_result = nopeui::xaml::Parse(input);
        if (parsed_result.ok()) {
            ++parsed;
            auto loaded_result = nopeui::xaml::Load(input);
            if (loaded_result.ok()) {
                ++loaded;
            }
        }
        ++iterations;

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_report) {
            const double elapsed =
                std::chrono::duration<double>(now - start).count();
            std::printf("[%6.1fs] iterations=%llu parse_ok=%llu load_ok=%llu rate=%.0f/s\n",
                elapsed,
                static_cast<unsigned long long>(iterations),
                static_cast<unsigned long long>(parsed),
                static_cast<unsigned long long>(loaded),
                elapsed > 0.0 ? static_cast<double>(iterations) / elapsed : 0.0);
            std::fflush(stdout);
            next_report = now + std::chrono::seconds(5);
        }

        if (max_iterations > 0.0 && static_cast<double>(iterations) >= max_iterations) {
            break;
        }
    }

    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("done: iterations=%llu parse_ok=%llu load_ok=%llu elapsed=%.1fs\n",
        static_cast<unsigned long long>(iterations),
        static_cast<unsigned long long>(parsed),
        static_cast<unsigned long long>(loaded),
        elapsed);
    return 0;
}
