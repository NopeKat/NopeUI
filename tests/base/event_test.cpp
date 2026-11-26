#include "../test_check.h"
#include <cstddef>
#include <utility>

#include "nopeui/base/event.h"

int main() {
    nopeui::Event<int> event;
    int sum = 0;

    auto first = event.Subscribe([&sum](int value) {
        sum += value;
    });
    NOPEUI_CHECK(event.HandlerCount() == std::size_t{1});

    {
        auto second = event.Subscribe([&sum](int value) {
            sum += value * 2;
        });
        event.Raise(3);
        NOPEUI_CHECK(sum == 9);
    }

    NOPEUI_CHECK(event.HandlerCount() == std::size_t{1});
    event.Raise(3);
    NOPEUI_CHECK(sum == 12);

    auto moved = std::move(first);
    first.Reset();
    NOPEUI_CHECK(event.HandlerCount() == std::size_t{1});
    NOPEUI_CHECK(static_cast<bool>(moved));

    moved.Reset();
    NOPEUI_CHECK(event.HandlerCount() == std::size_t{0});
    event.Raise(3);
    NOPEUI_CHECK(sum == 12);

    return 0;
}
