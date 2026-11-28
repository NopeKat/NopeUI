#include "nopeui/base/diagnostic.h"

#include <Windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <strsafe.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "nopeui/base/strings.h"
#include "nopeui/base/version.h"

namespace {

constexpr std::size_t kMinimumFileBytes = 256;
constexpr std::size_t kMaximumFileBytes = 256u * 1024u * 1024u;
constexpr unsigned kMaximumFiles = 16;
constexpr std::size_t kMaximumPathLength = 32768;
constexpr std::size_t kMaximumArchiveEntryBytes = 512u * 1024u * 1024u;

struct State {
    std::mutex mutex;
    std::FILE* file = nullptr;
    std::string directory_utf8;
    std::wstring directory_wide;
    std::size_t max_file_bytes = 2u * 1024u * 1024u;
    unsigned max_files = 3;
    bool capture_crashes = false;
    PVOID veh_handle = nullptr;
    std::thread watchdog;
};

State g_state;
wchar_t g_log_path[kMaximumPathLength]{};
LONG g_crash_guard = 0;
LONG g_hang_guard = 0;
std::atomic<bool> g_watchdog_stop{false};
std::mutex g_watchdog_mutex;
std::condition_variable g_watchdog_wakeup;

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

std::string WideToUtf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = static_cast<int>(text.size());
    const int bytes = ::WideCharToMultiByte(
        CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return {};
    }
    std::string output(static_cast<std::size_t>(bytes), '\0');
    ::WideCharToMultiByte(
        CP_UTF8, 0, text.data(), length, output.data(), bytes, nullptr, nullptr);
    return output;
}

struct OsVersionInfo {
    DWORD size;
    DWORD major;
    DWORD minor;
    DWORD build;
    DWORD platform;
    wchar_t service_pack[128];
};

bool QueryOsVersion(OsVersionInfo& info) noexcept {
    info = {};
    info.size = static_cast<DWORD>(sizeof(info));
    const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    using Function = LONG(WINAPI*)(OsVersionInfo*);
    const Function function = ntdll != nullptr
                                  ? reinterpret_cast<Function>(::GetProcAddress(ntdll, "RtlGetVersion"))
                                  : nullptr;
    return function != nullptr && function(&info) == 0;
}

std::string ResourceSummary() {
    const DWORD gdi = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    const DWORD user = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    ::GetProcessMemoryInfo(
        ::GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
        static_cast<DWORD>(sizeof(counters)));

    char text[144]{};
    std::snprintf(
        text,
        std::size(text),
        "gdi %lu, user %lu, working %llu MB, private %llu MB",
        static_cast<unsigned long>(gdi),
        static_cast<unsigned long>(user),
        static_cast<unsigned long long>(counters.WorkingSetSize / (1024ull * 1024ull)),
        static_cast<unsigned long long>(counters.PrivateUsage / (1024ull * 1024ull)));
    return text;
}

BOOL CALLBACK CountMonitor(HMONITOR, HDC, LPRECT, LPARAM parameter) {
    int* count = reinterpret_cast<int*>(parameter);
    ++*count;
    return TRUE;
}

std::string EnvironmentSummaryInternal() {
    std::string summary;
    summary += "nopeui: ";
    summary += nopeui::VersionString();
    summary += "\n";

    OsVersionInfo os{};
    summary += "os: ";
    if (QueryOsVersion(os)) {
        summary += std::to_string(os.major);
        summary += ".";
        summary += std::to_string(os.minor);
        summary += ".";
        summary += std::to_string(os.build);
        if (os.service_pack[0] != L'\0') {
            summary += " ";
            summary += WideToUtf8(os.service_pack);
        }
    } else {
        summary += "unknown";
    }
#if defined(_M_ARM64)
    summary += " (arm64)";
#elif defined(_M_X64)
    summary += " (x64)";
#else
    summary += " (x86)";
#endif
    summary += "\n";

    int monitors = 0;
    ::EnumDisplayMonitors(nullptr, nullptr, &CountMonitor, reinterpret_cast<LPARAM>(&monitors));
    summary += "display: dpi ";
    summary += std::to_string(::GetDpiForSystem());
    summary += ", monitors ";
    summary += std::to_string(monitors);
    summary += ", primary ";
    summary += std::to_string(::GetSystemMetrics(SM_CXSCREEN));
    summary += "x";
    summary += std::to_string(::GetSystemMetrics(SM_CYSCREEN));
    summary += "\n";

    summary += "session: ";
    summary += ::GetSystemMetrics(SM_REMOTESESSION) != 0 ? "remote" : "local";
    summary += "\n";

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    ::GlobalMemoryStatusEx(&memory);
    summary += "memory: total ";
    summary += std::to_string(
        static_cast<unsigned long long>(memory.ullTotalPhys / (1024ull * 1024ull)));
    summary += " MB, ";
    summary += ResourceSummary();
    summary += "\n";

    std::string command = WideToUtf8(::GetCommandLineW());
    if (command.size() > 400) {
        command.resize(400);
        command += "...";
    }
    summary += "command: ";
    summary += command;
    summary += "\n";
    return summary;
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

bool BuildDumpPath(const wchar_t* prefix, wchar_t* path, std::size_t capacity) noexcept {
    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    return SUCCEEDED(::StringCchPrintfW(
        path,
        capacity,
        L"%s\\%s-%04u%02u%02u-%02u%02u%02u.dmp",
        g_state.directory_wide.c_str(),
        prefix,
        static_cast<unsigned>(now.wYear),
        static_cast<unsigned>(now.wMonth),
        static_cast<unsigned>(now.wDay),
        static_cast<unsigned>(now.wHour),
        static_cast<unsigned>(now.wMinute),
        static_cast<unsigned>(now.wSecond)));
}

std::wstring CrashPath() {
    wchar_t path[512]{};
    if (!BuildDumpPath(L"crash", path, ARRAYSIZE(path))) {
        return {};
    }
    return path;
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

    wchar_t path[512]{};
    const bool built = BuildDumpPath(L"crash", path, ARRAYSIZE(path));
    const bool dumped = built && WriteMinidump(path, pointers);
    const DWORD dump_error = dumped
                                 ? 0u
                                 : (built ? ::GetLastError() : ERROR_INVALID_PARAMETER);

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

struct ProbeContext {
    DWORD process_id = 0;
    unsigned timeout_ms = 0;
    HWND hung = nullptr;
};

BOOL CALLBACK ProbeWindow(HWND hwnd, LPARAM parameter) {
    auto* context = reinterpret_cast<ProbeContext*>(parameter);
    DWORD process_id = 0;
    ::GetWindowThreadProcessId(hwnd, &process_id);
    if (process_id != context->process_id || ::IsWindowVisible(hwnd) == FALSE) {
        return TRUE;
    }

    DWORD_PTR result = 0;
    const LRESULT answered = ::SendMessageTimeoutW(
        hwnd,
        WM_NULL,
        0,
        0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK,
        context->timeout_ms,
        &result);
    if (answered == 0 && ::GetLastError() == ERROR_TIMEOUT) {
        context->hung = hwnd;
        return FALSE;
    }
    return TRUE;
}

void WriteHangDump() noexcept {
    if (::InterlockedCompareExchange(&g_hang_guard, 1, 0) != 0) {
        return;
    }
    wchar_t path[512]{};
    const bool built = BuildDumpPath(L"hang", path, ARRAYSIZE(path));
    const bool dumped = built && WriteMinidump(path, nullptr);
    nopeui::LogMessage(
        nopeui::LogLevel::Warning,
        dumped ? "hang minidump written" : "hang minidump failed");
    if (!dumped) {
        ::InterlockedExchange(&g_hang_guard, 0);
    }
}

void WatchdogLoop(unsigned hang_timeout_ms, unsigned resource_seconds) {
    bool hung = false;
    auto next_resources = std::chrono::steady_clock::now() +
                          std::chrono::seconds(resource_seconds > 0 ? resource_seconds : 1);

    while (!g_watchdog_stop.load(std::memory_order_relaxed)) {
        {
            std::unique_lock<std::mutex> lock(g_watchdog_mutex);
            g_watchdog_wakeup.wait_for(
                lock,
                std::chrono::milliseconds(250),
                [] { return g_watchdog_stop.load(std::memory_order_relaxed); });
        }
        if (g_watchdog_stop.load(std::memory_order_relaxed)) {
            break;
        }

        if (hang_timeout_ms > 0) {
            ProbeContext context{};
            context.process_id = ::GetCurrentProcessId();
            context.timeout_ms = hang_timeout_ms;
            ::EnumWindows(&ProbeWindow, reinterpret_cast<LPARAM>(&context));

            if (context.hung != nullptr) {
                if (!hung) {
                    hung = true;
                    nopeui::LogMessage(
                        nopeui::LogLevel::Warning,
                        "ui thread unresponsive for " + std::to_string(hang_timeout_ms) + " ms");
                    WriteHangDump();
                }
            } else if (hung) {
                hung = false;
                ::InterlockedExchange(&g_hang_guard, 0);
                nopeui::LogMessage(nopeui::LogLevel::Info, "ui thread responsive again");
            }
        }

        if (resource_seconds > 0 && std::chrono::steady_clock::now() >= next_resources) {
            nopeui::LogMessage(nopeui::LogLevel::Info, "resources: " + ResourceSummary());
            next_resources += std::chrono::seconds(resource_seconds);
        }
    }
}

void PutU16(std::string& out, unsigned value) {
    out.push_back(static_cast<char>(value & 0xFFu));
    out.push_back(static_cast<char>((value >> 8) & 0xFFu));
}

void PutU32(std::string& out, unsigned value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<char>((value >> shift) & 0xFFu));
    }
}

void DosDateTime(unsigned& date, unsigned& time) {
    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    date = ((static_cast<unsigned>(now.wYear) - 1980u) << 9) |
           (static_cast<unsigned>(now.wMonth) << 5) |
           static_cast<unsigned>(now.wDay);
    time = (static_cast<unsigned>(now.wHour) << 11) |
           (static_cast<unsigned>(now.wMinute) << 5) |
           (static_cast<unsigned>(now.wSecond) / 2u);
}

std::uint32_t Crc32(const unsigned char* data, std::size_t size) {
    static std::uint32_t table[256]{};
    static std::once_flag once;
    std::call_once(once, [] {
        for (std::uint32_t index = 0; index < 256; ++index) {
            std::uint32_t value = index;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1u) != 0 ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
            }
            table[index] = value;
        }
    });

    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t index = 0; index < size; ++index) {
        crc = table[(crc ^ data[index]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

struct ZipInput {
    std::string name;
    std::vector<unsigned char> data;
};

bool ReadFileBytes(const std::wstring& path, std::vector<unsigned char>& data) {
    const HANDLE file = ::CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        static_cast<std::size_t>(size.QuadPart) > kMaximumArchiveEntryBytes) {
        ::CloseHandle(file);
        return false;
    }

    data.resize(static_cast<std::size_t>(size.QuadPart));
    std::size_t offset = 0;
    while (offset < data.size()) {
        DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, 1u << 20));
        DWORD read = 0;
        if (!::ReadFile(file, data.data() + offset, chunk, &read, nullptr) || read == 0) {
            ::CloseHandle(file);
            return false;
        }
        offset += read;
    }
    ::CloseHandle(file);
    return true;
}

bool CollectDiagnosticFiles(const std::wstring& directory, std::vector<ZipInput>& inputs) {
    WIN32_FIND_DATAW entry{};
    const std::wstring pattern = directory + L"\\*";
    const HANDLE find = ::FindFirstFileW(pattern.c_str(), &entry);
    if (find == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        const std::wstring name = entry.cFileName;
        const bool log = name.starts_with(L"diagnostics") && name.ends_with(L".log");
        const bool dump = name.ends_with(L".dmp");
        if (!log && !dump) {
            continue;
        }

        ZipInput input;
        input.name = WideToUtf8(name);
        if (!ReadFileBytes(directory + L"\\" + name, input.data)) {
            continue;
        }
        inputs.push_back(std::move(input));
    } while (::FindNextFileW(find, &entry) != FALSE);

    ::FindClose(find);
    std::sort(
        inputs.begin(),
        inputs.end(),
        [](const ZipInput& left, const ZipInput& right) { return left.name < right.name; });
    return true;
}

bool WriteArchive(const std::wstring& path, const std::vector<ZipInput>& inputs) {
    struct CentralRecord {
        std::string name;
        std::uint32_t crc = 0;
        std::uint32_t size = 0;
        std::uint32_t offset = 0;
    };

    unsigned dos_date = 0;
    unsigned dos_time = 0;
    DosDateTime(dos_date, dos_time);

    std::string archive;
    std::vector<CentralRecord> central;
    central.reserve(inputs.size());

    for (const ZipInput& input : inputs) {
        CentralRecord record;
        record.name = input.name;
        record.crc = Crc32(input.data.data(), input.data.size());
        record.size = static_cast<std::uint32_t>(input.data.size());
        record.offset = static_cast<std::uint32_t>(archive.size());
        central.push_back(record);

        PutU32(archive, 0x04034b50u);
        PutU16(archive, 20);
        PutU16(archive, 0x0800u);
        PutU16(archive, 0);
        PutU16(archive, dos_time);
        PutU16(archive, dos_date);
        PutU32(archive, record.crc);
        PutU32(archive, record.size);
        PutU32(archive, record.size);
        PutU16(archive, static_cast<unsigned>(input.name.size()));
        PutU16(archive, 0);
        archive.append(input.name);
        if (!input.data.empty()) {
            archive.append(
                reinterpret_cast<const char*>(input.data.data()), input.data.size());
        }
    }

    const std::uint32_t central_offset = static_cast<std::uint32_t>(archive.size());
    for (const CentralRecord& record : central) {
        PutU32(archive, 0x02014b50u);
        PutU16(archive, 20);
        PutU16(archive, 20);
        PutU16(archive, 0x0800u);
        PutU16(archive, 0);
        PutU16(archive, dos_time);
        PutU16(archive, dos_date);
        PutU32(archive, record.crc);
        PutU32(archive, record.size);
        PutU32(archive, record.size);
        PutU16(archive, static_cast<unsigned>(record.name.size()));
        PutU16(archive, 0);
        PutU16(archive, 0);
        PutU16(archive, 0);
        PutU16(archive, 0);
        PutU32(archive, 0);
        PutU32(archive, record.offset);
        archive.append(record.name);
    }

    const std::uint32_t central_size =
        static_cast<std::uint32_t>(archive.size()) - central_offset;
    PutU32(archive, 0x06054b50u);
    PutU16(archive, 0);
    PutU16(archive, 0);
    PutU16(archive, static_cast<unsigned>(central.size()));
    PutU16(archive, static_cast<unsigned>(central.size()));
    PutU32(archive, central_size);
    PutU32(archive, central_offset);
    PutU16(archive, 0);

    const HANDLE file = ::CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const BOOL result = ::WriteFile(
        file,
        archive.data(),
        static_cast<DWORD>(archive.size()),
        &written,
        nullptr);
    ::CloseHandle(file);
    return result != FALSE && written == archive.size();
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

        // Before the first window the process may not have declared DPI
        // awareness, and a DPI query would then answer 96. Declare the
        // framework's per-monitor mode so the snapshot reports the real DPI.
        ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        std::string environment = EnvironmentSummaryInternal();
        std::string compact;
        compact.reserve(environment.size());
        for (const char character : environment) {
            if (character == '\n') {
                compact += " | ";
            } else {
                compact.push_back(character);
            }
        }
        while (compact.size() >= 3 && compact.compare(compact.size() - 3, 3, " | ") == 0) {
            compact.resize(compact.size() - 3);
        }
        WriteLineLocked(LogLevel::Info, "environment: " + compact);

        const unsigned hang_timeout = options.detect_hangs ? options.hang_timeout_ms : 0u;
        const unsigned resource_seconds = options.resource_interval_seconds;
        if (hang_timeout > 0 || resource_seconds > 0) {
            g_watchdog_stop.store(false);
            g_state.watchdog = std::thread(WatchdogLoop, hang_timeout, resource_seconds);
        }
        return Result<void>::Ok();
    } catch (...) {
        return Result<void>::Fail(
            Error{ErrorCode::PlatformFailure, "diagnostics start failed"});
    }
}

void StopDiagnostics() noexcept {
    std::thread watchdog;
    try {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (g_state.file == nullptr) {
            return;
        }
        WriteLineLocked(LogLevel::Info, "diagnostics stopped");
        CloseLogFile();
        // The header promises an empty directory once diagnostics are stopped.
        g_state.directory_utf8.clear();
        g_state.directory_wide.clear();
        if (g_state.capture_crashes) {
            g_state.capture_crashes = false;
            ::SetUnhandledExceptionFilter(nullptr);
            if (g_state.veh_handle != nullptr) {
                ::RemoveVectoredExceptionHandler(g_state.veh_handle);
                g_state.veh_handle = nullptr;
            }
        }
        g_watchdog_stop.store(true);
        if (g_state.watchdog.joinable()) {
            watchdog = std::move(g_state.watchdog);
        }
    } catch (...) {
    }
    g_watchdog_wakeup.notify_all();
    if (watchdog.joinable()) {
        watchdog.join();
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

std::string EnvironmentSummary() noexcept {
    try {
        return EnvironmentSummaryInternal();
    } catch (...) {
        return {};
    }
}

Result<std::string> ExportDiagnostics(std::string_view note) noexcept {
    try {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (g_state.directory_wide.empty()) {
            return Result<std::string>::Fail(
                Error{ErrorCode::InvalidState, "diagnostics are not started"});
        }

        SYSTEMTIME now{};
        ::GetLocalTime(&now);
        wchar_t name[64]{};
        std::swprintf(
            name,
            std::size(name),
            L"diagnostics-%04u%02u%02u-%02u%02u%02u.zip",
            static_cast<unsigned>(now.wYear),
            static_cast<unsigned>(now.wMonth),
            static_cast<unsigned>(now.wDay),
            static_cast<unsigned>(now.wHour),
            static_cast<unsigned>(now.wMinute),
            static_cast<unsigned>(now.wSecond));
        const std::wstring path = g_state.directory_wide + L"\\" + name;

        std::vector<ZipInput> inputs;
        if (!CollectDiagnosticFiles(g_state.directory_wide, inputs)) {
            return Result<std::string>::Fail(
                Error{ErrorCode::PlatformFailure, "diagnostic directory cannot be read"});
        }

        std::string environment = EnvironmentSummaryInternal();
        environment += "exported: ";
        environment += Timestamp();
        environment += "\n";
        if (!note.empty()) {
            environment += "note: ";
            environment += note;
            environment += "\n";
        }

        ZipInput environment_entry;
        environment_entry.name = "environment.txt";
        environment_entry.data.assign(environment.begin(), environment.end());
        inputs.push_back(std::move(environment_entry));

        if (!WriteArchive(path, inputs)) {
            return Result<std::string>::Fail(
                Error{ErrorCode::PlatformFailure, "diagnostic archive cannot be written"});
        }

        const std::string utf8_path = WideToUtf8(path);
        WriteLineLocked(LogLevel::Info, "diagnostics exported: " + utf8_path);
        return Result<std::string>::Ok(utf8_path);
    } catch (...) {
        return Result<std::string>::Fail(
            Error{ErrorCode::PlatformFailure, "diagnostic archive cannot be written"});
    }
}

}  // namespace nopeui
