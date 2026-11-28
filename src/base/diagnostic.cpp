#include "nopeui/base/diagnostic.h"

#include <Windows.h>
#include <dbghelp.h>
#include <strsafe.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "nopeui/base/strings.h"
#include "nopeui/base/version.h"

namespace {

constexpr std::size_t kMinimumFileBytes = 256;
constexpr std::size_t kMaximumFileBytes = 256u * 1024u * 1024u;
constexpr unsigned kMaximumFiles = 16;
constexpr std::size_t kMaximumPathLength = 32768;

struct State {
    std::mutex mutex;
    std::FILE* file = nullptr;
    std::string directory_utf8;
    std::wstring directory_wide;
    std::size_t max_file_bytes = 2u * 1024u * 1024u;
    unsigned max_files = 3;
    bool capture_crashes = false;
    PVOID veh_handle = nullptr;
};

State g_state;
wchar_t g_log_path[kMaximumPathLength]{};
LONG g_crash_guard = 0;

std::string_view LevelName(nopeui::LogLevel level) noexcept {
    switch (level) {
        case nopeui::LogLevel::Trace:
            return "trace";
        case nopeui::LogLevel::Info:
            return "info";
        case nopeui::LogLevel::Warning:
            return "warning";
        case nopeui::LogLevel::Error:
            return "error";
        case nopeui::LogLevel::Fatal:
            return "fatal";
    }
    return "info";
}

std::string Timestamp() {
    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    char text[32]{};
    std::snprintf(
        text,
        std::size(text),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        static_cast<unsigned>(now.wYear),
        static_cast<unsigned>(now.wMonth),
        static_cast<unsigned>(now.wDay),
        static_cast<unsigned>(now.wHour),
        static_cast<unsigned>(now.wMinute),
        static_cast<unsigned>(now.wSecond),
        static_cast<unsigned>(now.wMilliseconds));
    return text;
}

std::wstring LogPath(unsigned index) {
    std::wstring path = g_state.directory_wide;
    path += L"\\diagnostics";
    if (index != 0) {
        path += L".";
        path += std::to_wstring(index);
    }
    path += L".log";
    return path;
}

std::FILE* OpenLogFile(const std::wstring& path) noexcept {
    // Sharing stays open so a report tool can copy the log while the app runs,
    // and the crash filter can append its last line through Win32.
    return _wfsopen(path.c_str(), L"ab", _SH_DENYNO);
}

void CloseLogFile() noexcept {
    if (g_state.file != nullptr) {
        std::fclose(g_state.file);
        g_state.file = nullptr;
    }
}

void RotateLogLocked();

void WriteLineLocked(nopeui::LogLevel level, std::string_view message) {
    if (g_state.file == nullptr) {
        return;
    }

    std::string line;
    line.reserve(message.size() + 48);
    line += Timestamp();
    line += " [";
    line += LevelName(level);
    line += "] ";
    line += message;
    line += "\n";

    std::fwrite(line.data(), 1, line.size(), g_state.file);
    std::fflush(g_state.file);
    const long size = std::ftell(g_state.file);
    if (size >= 0 && static_cast<std::size_t>(size) >= g_state.max_file_bytes) {
        RotateLogLocked();
    }
}

void RotateLogLocked() {
    CloseLogFile();
    for (unsigned index = g_state.max_files; index > 1; --index) {
        ::MoveFileExW(
            LogPath(index - 1).c_str(),
            LogPath(index).c_str(),
            MOVEFILE_REPLACE_EXISTING);
    }
    const std::wstring current = LogPath(0);
    ::MoveFileExW(current.c_str(), LogPath(1).c_str(), MOVEFILE_REPLACE_EXISTING);
    g_state.file = OpenLogFile(current);
}

bool CreateDirectoryTree(const std::wstring& path) noexcept {
    if (path.empty()) {
        return false;
    }
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    const std::size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos && slash > 0) {
        if (!CreateDirectoryTree(path.substr(0, slash))) {
            return false;
        }
    }
    if (::CreateDirectoryW(path.c_str(), nullptr) != 0) {
        return true;
    }
    return ::GetLastError() == ERROR_ALREADY_EXISTS;
}

nopeui::Result<std::wstring> ResolveDirectory(std::string_view directory) {
    if (!directory.empty()) {
        auto converted = nopeui::Utf8ToUtf16(directory);
        if (!converted.ok()) {
            return nopeui::Result<std::wstring>::Fail(converted.error());
        }
        return nopeui::Result<std::wstring>::Ok(
            std::wstring(converted.value().begin(), converted.value().end()));
    }

    wchar_t local_app_data[kMaximumPathLength]{};
    const DWORD length = ::GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        local_app_data,
        static_cast<DWORD>(kMaximumPathLength));
    if (length == 0 || static_cast<std::size_t>(length) >= kMaximumPathLength) {
        return nopeui::Result<std::wstring>::Fail(
            nopeui::Error{nopeui::ErrorCode::PlatformFailure, "LOCALAPPDATA is not available"});
    }
    return nopeui::Result<std::wstring>::Ok(
        std::wstring(local_app_data) + L"\\NopeUI\\diagnostics");
}

std::wstring CrashPath() {
    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    wchar_t name[64]{};
    std::swprintf(
        name,
        std::size(name),
        L"crash-%04u%02u%02u-%02u%02u%02u.dmp",
        static_cast<unsigned>(now.wYear),
        static_cast<unsigned>(now.wMonth),
        static_cast<unsigned>(now.wDay),
        static_cast<unsigned>(now.wHour),
        static_cast<unsigned>(now.wMinute),
        static_cast<unsigned>(now.wSecond));
    return g_state.directory_wide + L"\\" + name;
}

bool WriteMinidump(const wchar_t* path, EXCEPTION_POINTERS* pointers) noexcept {
    const HANDLE file = ::CreateFileW(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION exception{};
    exception.ThreadId = ::GetCurrentThreadId();
    exception.ExceptionPointers = pointers;
    exception.ClientPointers = FALSE;
    const MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo);

    const BOOL written = ::MiniDumpWriteDump(
        ::GetCurrentProcess(),
        ::GetCurrentProcessId(),
        file,
        type,
        pointers != nullptr ? &exception : nullptr,
        nullptr,
        nullptr);
    ::CloseHandle(file);
    return written != FALSE;
}

void AppendCrashLogLine(DWORD code, bool dumped, DWORD dump_error) noexcept {
    if (g_log_path[0] == L'\0') {
        return;
    }
    const HANDLE log = ::CreateFileW(
        g_log_path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (log == INVALID_HANDLE_VALUE) {
        return;
    }

    char line[200]{};
    int length = 0;
    if (dumped) {
        length = std::snprintf(
            line,
            std::size(line),
            "nopeui [fatal] unhandled exception 0x%08X, minidump written\n",
            static_cast<unsigned>(code));
    } else {
        length = std::snprintf(
            line,
            std::size(line),
            "nopeui [fatal] unhandled exception 0x%08X, minidump failed (%lu)\n",
            static_cast<unsigned>(code),
            static_cast<unsigned long>(dump_error));
    }
    if (length > 0) {
        DWORD ignored = 0;
        ::WriteFile(log, line, static_cast<DWORD>(length), &ignored, nullptr);
    }
    ::CloseHandle(log);
}

bool IsCrashCode(DWORD code) noexcept {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        case EXCEPTION_PRIV_INSTRUCTION:
            return true;
        default:
            return false;
    }
}

// Writes one dump at most: the first-chance vector and the top-level filter
// can both arrive for the same exception.
bool CaptureCrash(EXCEPTION_POINTERS* pointers) noexcept {
    if (!g_state.capture_crashes || g_state.directory_wide.empty()) {
        return false;
    }
    if (pointers == nullptr || pointers->ExceptionRecord == nullptr) {
        return false;
    }
    if (::InterlockedCompareExchange(&g_crash_guard, 1, 0) != 0) {
        return false;
    }

    const DWORD code = pointers->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_STACK_OVERFLOW) {
        AppendCrashLogLine(code, false, ERROR_STACK_OVERFLOW);
        return false;
    }

    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    wchar_t path[512]{};
    const HRESULT written_path = ::StringCchPrintfW(
        path,
        ARRAYSIZE(path),
        L"%s\\crash-%04u%02u%02u-%02u%02u%02u.dmp",
        g_state.directory_wide.c_str(),
        static_cast<unsigned>(now.wYear),
        static_cast<unsigned>(now.wMonth),
        static_cast<unsigned>(now.wDay),
        static_cast<unsigned>(now.wHour),
        static_cast<unsigned>(now.wMinute),
        static_cast<unsigned>(now.wSecond));
    const bool dumped = SUCCEEDED(written_path) && WriteMinidump(path, pointers);
    const DWORD dump_error = dumped
                                 ? 0u
                                 : (SUCCEEDED(written_path) ? ::GetLastError()
                                                           : static_cast<DWORD>(written_path));

    AppendCrashLogLine(code, dumped, dump_error);
    return dumped;
}

LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* pointers) noexcept {
    return CaptureCrash(pointers) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

// Windows 11 builds do not always call the top-level filter for an access
// violation, so the dump is also taken on the first-chance path. The guard in
// CaptureCrash keeps a single dump and stops the handler from recursing.
LONG WINAPI OnFirstChanceException(EXCEPTION_POINTERS* pointers) noexcept {
    if (pointers == nullptr || pointers->ExceptionRecord == nullptr) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (!IsCrashCode(pointers->ExceptionRecord->ExceptionCode)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    CaptureCrash(pointers);
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

namespace nopeui {

Result<void> StartDiagnostics(DiagnosticOptions options) noexcept {
    try {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (g_state.file != nullptr) {
            return Result<void>::Ok();
        }

        auto directory = ResolveDirectory(options.directory);
        if (!directory.ok()) {
            return Result<void>::Fail(directory.error());
        }
        if (!CreateDirectoryTree(directory.value())) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, "diagnostic directory cannot be created"});
        }

        g_state.directory_wide = std::move(directory).value();
        std::u16string utf16;
        utf16.reserve(g_state.directory_wide.size());
        for (const wchar_t character : g_state.directory_wide) {
            utf16.push_back(static_cast<char16_t>(character));
        }
        auto utf8 = Utf16ToUtf8(utf16);
        if (!utf8.ok()) {
            return Result<void>::Fail(utf8.error());
        }
        g_state.directory_utf8 = std::move(utf8).value();

        g_state.max_file_bytes = std::clamp(
            options.max_file_bytes, kMinimumFileBytes, kMaximumFileBytes);
        g_state.max_files = std::clamp(options.max_files, 1u, kMaximumFiles);

        const std::wstring log_path = LogPath(0);
        std::swprintf(g_log_path, kMaximumPathLength, L"%ls", log_path.c_str());
        g_state.file = OpenLogFile(log_path);
        if (g_state.file == nullptr) {
            return Result<void>::Fail(
                Error{ErrorCode::PlatformFailure, "diagnostics.log cannot be opened"});
        }

        g_state.capture_crashes = options.capture_crashes;
        if (g_state.capture_crashes) {
            g_state.veh_handle = ::AddVectoredExceptionHandler(1, &OnFirstChanceException);
            ::SetUnhandledExceptionFilter(&OnUnhandledException);
        }

        WriteLineLocked(
            LogLevel::Info,
            std::string("NopeUI ") + VersionString() + " diagnostics started");
        WriteLineLocked(LogLevel::Info, g_state.directory_utf8);
        return Result<void>::Ok();
    } catch (...) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, "diagnostics start failed"});
    }
}

void StopDiagnostics() noexcept {
    try {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (g_state.file == nullptr) {
            return;
        }
        WriteLineLocked(LogLevel::Info, "diagnostics stopped");
        CloseLogFile();
        if (g_state.capture_crashes) {
            g_state.capture_crashes = false;
            ::SetUnhandledExceptionFilter(nullptr);
            if (g_state.veh_handle != nullptr) {
                ::RemoveVectoredExceptionHandler(g_state.veh_handle);
                g_state.veh_handle = nullptr;
            }
        }
    } catch (...) {
    }
}

void LogMessage(LogLevel level, std::string_view message) noexcept {
    try {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (g_state.file == nullptr) {
            std::string debug = "NopeUI [";
            debug += LevelName(level);
            debug += "] ";
            debug += message;
            ::OutputDebugStringA(debug.c_str());
            return;
        }
        WriteLineLocked(level, message);
    } catch (...) {
        ::OutputDebugStringA("NopeUI: log write failed\n");
    }
}

std::string DiagnosticsDirectory() noexcept {
    try {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        return g_state.directory_utf8;
    } catch (...) {
        return {};
    }
}

Result<std::string> WriteDump(std::string_view reason) noexcept {
    try {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (g_state.directory_wide.empty()) {
            return Result<std::string>::Fail(
                Error{ErrorCode::InvalidState, "diagnostics are not started"});
        }

        const std::wstring path = CrashPath();
        if (!WriteMinidump(path.c_str(), nullptr)) {
            return Result<std::string>::Fail(
                Error{ErrorCode::PlatformFailure, "minidump cannot be written"});
        }

        std::u16string utf16;
        utf16.reserve(path.size());
        for (const wchar_t character : path) {
            utf16.push_back(static_cast<char16_t>(character));
        }
        auto utf8 = Utf16ToUtf8(utf16);
        if (!utf8.ok()) {
            return Result<std::string>::Fail(utf8.error());
        }

        std::string message = "manual minidump: ";
        message += utf8.value();
        if (!reason.empty()) {
            message += " (";
            message += reason;
            message += ")";
        }
        WriteLineLocked(LogLevel::Error, message);
        return Result<std::string>::Ok(std::move(utf8).value());
    } catch (...) {
        return Result<std::string>::Fail(
            Error{ErrorCode::PlatformFailure, "minidump cannot be written"});
    }
}

}  // namespace nopeui
