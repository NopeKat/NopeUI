#ifndef NOPEUI_BASE_RESULT_H_
#define NOPEUI_BASE_RESULT_H_

#include <cassert>
#include <optional>
#include <utility>

#include "nopeui/base/error.h"

namespace nopeui {

template <typename T>
class Result {
public:
    static Result Ok(T value) {
        Result result;
        result.value_.emplace(std::move(value));
        return result;
    }

    static Result Fail(Error error) noexcept {
        assert(error.code != ErrorCode::None);
        Result result;
        result.error_ = std::move(error);
        return result;
    }

    bool ok() const noexcept {
        return value_.has_value();
    }

    explicit operator bool() const noexcept {
        return ok();
    }

    T& value() & noexcept {
        assert(value_.has_value());
        return *value_;
    }

    const T& value() const& noexcept {
        assert(value_.has_value());
        return *value_;
    }

    T&& value() && noexcept {
        assert(value_.has_value());
        return std::move(*value_);
    }

    const Error& error() const noexcept {
        return error_;
    }

private:
    Result() = default;

    std::optional<T> value_;
    Error error_;
};

template <>
class Result<void> {
public:
    static Result Ok() noexcept {
        return Result{};
    }

    static Result Fail(Error error) noexcept {
        assert(error.code != ErrorCode::None);
        Result result;
        result.error_ = std::move(error);
        return result;
    }

    bool ok() const noexcept {
        return error_.code == ErrorCode::None;
    }

    explicit operator bool() const noexcept {
        return ok();
    }

    const Error& error() const noexcept {
        return error_;
    }

private:
    Result() = default;

    Error error_;
};

}  // namespace nopeui

#endif  // NOPEUI_BASE_RESULT_H_
