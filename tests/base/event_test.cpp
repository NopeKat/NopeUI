#include <cassert>
#include <cstddef>
#include <utility>

#include "nopeui/base/event.h"

int main() {
    nopeui::Event<int> event;
    int sum = 0;

    auto first = event.Subscribe([&sum](int value) {
        sum += value;
    });
    assert(event.HandlerCount() == std::size_t{1});

    {
        auto second = event.Subscribe([&sum](int value) {
            sum += value * 2;
        });
        event.Raise(3);
        assert(sum == 9);
    }

    assert(event.HandlerCount() == std::size_t{1});
    event.Raise(3);
    assert(sum == 12);

    auto moved = std::move(first);
    first.Reset();
    assert(event.HandlerCount() == std::size_t{1});
    assert(static_cast<bool>(moved));

    moved.Reset();
    assert(event.HandlerCount() == std::size_t{0});
    event.Raise(3);
    assert(sum == 12);

    return 0;
}
