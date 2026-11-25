#include <cassert>

#include "nopeui/base/result.h"

int main() {
    const auto ok = nopeui::Result<int>::Ok(42);
    assert(ok.ok());
    assert(ok.value() == 42);
    assert(ok.error().code == nopeui::ErrorCode::None);

    const auto failed = nopeui::Result<int>::Fail(
        nopeui::Error{nopeui::ErrorCode::InvalidArgument, "bad argument"});
    assert(!failed.ok());
    assert(failed.error().code == nopeui::ErrorCode::InvalidArgument);
    assert(failed.error().message == "bad argument");

    const auto void_ok = nopeui::Result<void>::Ok();
    assert(void_ok.ok());
    assert(void_ok.error().code == nopeui::ErrorCode::None);

    const auto void_failed = nopeui::Result<void>::Fail(
        nopeui::Error{nopeui::ErrorCode::InvalidState, "bad state"});
    assert(!void_failed.ok());
    assert(void_failed.error().code == nopeui::ErrorCode::InvalidState);
    assert(void_failed.error().message == "bad state");

    return 0;
}
