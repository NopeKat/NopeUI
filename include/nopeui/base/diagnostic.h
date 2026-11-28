#ifndef NOPEUI_BASE_DIAGNOSTIC_H_
#define NOPEUI_BASE_DIAGNOSTIC_H_

#include <cstddef>
#include <string>
#include <string_view>

#include "nopeui/base/result.h"

namespace nopeui {

enum class LogLevel {
    Trace = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4,
};

struct DiagnosticOptions {
    // UTF-8 directory for the log and the dumps. Empty picks
    // %LOCALAPPDATA%\NopeUI\diagnostics.
    std::string directory;
    // The current log rolls to diagnostics.1.log once it reaches this size.
    std::size_t max_file_bytes = 2u * 1024u * 1024u;
    // Rotated files kept besides the current diagnostics.log.
    unsigned max_files = 3;
    // Installs the crash capture: a minidump on a fatal exception.
    bool capture_crashes = true;
    // Watches the windows of this process and dumps once when the UI thread
    // stops answering. A zero hang_timeout_ms turns the watch off.
    bool detect_hangs = true;
    unsigned hang_timeout_ms = 500;
    // Seconds between resource lines (GDI and USER handles, working set).
    // Zero turns the sampler off.
    unsigned resource_interval_seconds = 300;
};

// Creates the directory, opens diagnostics.log, writes a start line, and
// optionally installs the crash filter. Everything stays local: no telemetry,
// no upload. Calling it again while it is running returns Ok without changing
// anything; call StopDiagnostics first to switch directories.
Result<void> StartDiagnostics(DiagnosticOptions options = {}) noexcept;

// Writes a stop line, closes the log, and removes the crash filter.
void StopDiagnostics() noexcept;

// Appends one line with a local timestamp. Silent when diagnostics are not
// started; the text still goes to the debugger through OutputDebugString.
void LogMessage(LogLevel level, std::string_view message) noexcept;

// The active directory in UTF-8, empty when diagnostics are not started.
std::string DiagnosticsDirectory() noexcept;

// Writes a minidump by hand, for a report the user chooses to export, and
// records it in the log. Returns the UTF-8 path of the dump.
Result<std::string> WriteDump(std::string_view reason) noexcept;

// Multi-line snapshot: version, OS build, architecture, DPI, monitors,
// session, memory, handle counts, and the command line.
std::string EnvironmentSummary() noexcept;

// Zips diagnostics.log with its rotated files, every dump in the directory,
// and a freshly written environment.txt. Returns the UTF-8 path of the archive.
Result<std::string> ExportDiagnostics(std::string_view note = {}) noexcept;

}  // namespace nopeui

#endif  // NOPEUI_BASE_DIAGNOSTIC_H_
