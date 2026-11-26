#include "../test_check.h"

#include "nopeui/base/result.h"

int main() {
    const auto ok = nopeui::Result<int>::Ok(42);
    NOPEUI_CHECK(ok.ok());
    NOPEUI_CHECK(ok.value() == 42);
    NOPEUI_CHECK(ok.error().code == nopeui::ErrorCode::None);

    const auto failed = nopeui::Result<int>::Fail(
        nopeui::Error{nopeui::ErrorCode::InvalidArgument, "bad argument"});
    NOPEUI_CHECK(!failed.ok());
    NOPEUI_CHECK(failed.error().code == nopeui::ErrorCode::InvalidArgument);
    NOPEUI_CHECK(failed.error().message == "bad argument");

    const auto void_ok = nopeui::Result<void>::Ok();
    NOPEUI_CHECK(void_ok.ok());
    NOPEUI_CHECK(void_ok.error().code == nopeui::ErrorCode::None);

    const auto void_failed = nopeui::Result<void>::Fail(
        nopeui::Error{nopeui::ErrorCode::InvalidState, "bad state"});
    NOPEUI_CHECK(!void_failed.ok());
    NOPEUI_CHECK(void_failed.error().code == nopeui::ErrorCode::InvalidState);
    NOPEUI_CHECK(void_failed.error().message == "bad state");

    return 0;
}
