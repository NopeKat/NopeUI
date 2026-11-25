#ifndef NOPEUI_BASE_ERROR_H_
#define NOPEUI_BASE_ERROR_H_

#include <cstdint>
#include <string>

namespace nopeui {

enum class ErrorCode : std::uint32_t {
    None = 0,
    InvalidArgument = 1,
    OutOfRange = 2,
    InvalidState = 101,
    NotFound = 102,
    AlreadyExists = 103,
    OutOfMemory = 201,
    ResourceBusy = 202,
    PlatformFailure = 301,
    Unsupported = 302,
    ParseFailure = 401,
    InvalidUtf8 = 402,
    InvalidUtf16 = 403,
};

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;
};

}  // namespace nopeui

#endif  // NOPEUI_BASE_ERROR_H_
