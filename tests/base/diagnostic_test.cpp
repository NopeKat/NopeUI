// Checks the diagnostics API the fuzz runs depend on: start is idempotent, the
// log rotates, the environment snapshot carries the fields a report needs, a
// manual dump lands on disk, and the export produces a readable archive.

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "nopeui/base/diagnostic.h"

#include "../test_check.h"

namespace {

namespace fs = std::filesystem;

std::wstring TempDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    ::GetTempPathW(MAX_PATH, buffer);
    const std::wstring root(buffer);
    const std::wstring path =
        root + L"NopeUI-diagnostic-test-" + std::to_wstring(::GetCurrentProcessId());
    ::CreateDirectoryW(path.c_str(), nullptr);
    return path;
}

bool Contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
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

int RunCrashChild() {
    wchar_t value[MAX_PATH] = {};
    const DWORD size = ::GetEnvironmentVariableW(
        L"NOPEUI_DIAG_TEST_DIR", value, MAX_PATH);
    if (size == 0 || size >= MAX_PATH) {
        return 2;
    }
    nopeui::DiagnosticOptions options;
    options.directory = Utf8(std::wstring(value, size));
    options.max_file_bytes = 256u * 1024u;
    options.max_files = 2;
    options.detect_hangs = false;
    options.resource_interval_seconds = 0;
    if (!nopeui::StartDiagnostics(options).ok()) {
        return 2;
    }
    nopeui::LogMessage(nopeui::LogLevel::Info, "child about to crash");
    ::RaiseException(0xC0000005u, 0, 0, nullptr);
    return 3;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--crash-child") {
        return RunCrashChild();
    }

    const std::wstring temp = TempDirectory();
    const std::string directory = Utf8(temp);

    nopeui::DiagnosticOptions options;
    options.directory = directory;
    options.max_file_bytes = 4u * 1024u;
    options.max_files = 2;
    options.detect_hangs = false;
    options.resource_interval_seconds = 0;
    NOPEUI_CHECK(nopeui::StartDiagnostics(options).ok());

    // A second start must not reset the log or change the directory.
    NOPEUI_CHECK(nopeui::StartDiagnostics(options).ok());
    NOPEUI_CHECK(nopeui::DiagnosticsDirectory() == directory);

    for (int index = 0; index < 400; ++index) {
        char line[64] = {};
        std::snprintf(line, sizeof(line), "diagnostic test line %04d padding", index);
        nopeui::LogMessage(nopeui::LogLevel::Info, line);
    }

    const fs::path log_path = fs::path(temp) / "diagnostics.log";
    NOPEUI_CHECK(fs::exists(log_path));
    const fs::path rotated = fs::path(temp) / "diagnostics.1.log";
    NOPEUI_CHECK(fs::exists(rotated));

    const std::string environment = nopeui::EnvironmentSummary();
    NOPEUI_CHECK(Contains(environment, "nopeui: "));
    NOPEUI_CHECK(Contains(environment, "os: "));
    NOPEUI_CHECK(Contains(environment, "display: dpi "));
    NOPEUI_CHECK(Contains(environment, "memory: total "));

    auto dump = nopeui::WriteDump("diagnostic test");
    NOPEUI_CHECK(dump.ok());
    NOPEUI_CHECK(fs::exists(fs::path(dump.value())));
    NOPEUI_CHECK(fs::file_size(fs::path(dump.value())) > 1024);

    auto package = nopeui::ExportDiagnostics("diagnostic test");
    NOPEUI_CHECK(package.ok());
    NOPEUI_CHECK(fs::exists(fs::path(package.value())));
    NOPEUI_CHECK(fs::file_size(fs::path(package.value())) > 0);

    nopeui::StopDiagnostics();
    NOPEUI_CHECK(nopeui::DiagnosticsDirectory().empty());

    // Logging after stop is a silent no-op, not a crash.
    nopeui::LogMessage(nopeui::LogLevel::Info, "after stop");

    // The crash path runs in a child process, so this test can assert on the
    // dump and the fatal line without taking itself down.
    {
        ::SetEnvironmentVariableW(L"NOPEUI_DIAG_TEST_DIR", temp.c_str());
        wchar_t module[MAX_PATH] = {};
        ::GetModuleFileNameW(nullptr, module, MAX_PATH);
        std::wstring command = L"\"" + std::wstring(module) + L"\" --crash-child";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        NOPEUI_CHECK(::CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0,
            nullptr, nullptr, &startup, &process) != FALSE);
        ::SetEnvironmentVariableW(L"NOPEUI_DIAG_TEST_DIR", nullptr);
        ::WaitForSingleObject(process.hProcess, 30000);
        DWORD exit_code = 0;
        ::GetExitCodeProcess(process.hProcess, &exit_code);
        ::CloseHandle(process.hThread);
        ::CloseHandle(process.hProcess);
        NOPEUI_CHECK(exit_code == 0xC0000005u);
    }

    bool fatal_line = false;
    for (int index = 0; index <= 2; ++index) {
        const fs::path candidate = index == 0
            ? fs::path(temp) / "diagnostics.log"
            : fs::path(temp) / ("diagnostics." + std::to_string(index) + ".log");
        std::ifstream in(candidate, std::ios::binary);
        if (!in) {
            continue;
        }
        const std::string text((std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());
        if (Contains(text, "[fatal] unhandled exception")) {
            fatal_line = true;
        }
    }
    NOPEUI_CHECK(fatal_line);

    bool crash_dump = false;
    for (const auto& entry : fs::directory_iterator(fs::path(temp))) {
        if (entry.is_regular_file() && entry.path().extension() == ".dmp" &&
            entry.path().filename().string().rfind("crash-", 0) == 0 &&
            entry.file_size() > 1024) {
            crash_dump = true;
        }
    }
    NOPEUI_CHECK(crash_dump);

    std::error_code ignored;
    fs::remove_all(fs::path(temp), ignored);
    return 0;
}
