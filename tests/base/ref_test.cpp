#include <cassert>

#include "nopeui/base/ref.h"

namespace {

struct Widget : nopeui::RefCounted {
    static int destroyed;

    int value = 0;

    ~Widget() override {
        ++destroyed;
    }
};

int Widget::destroyed = 0;

struct DerivedWidget : Widget {};

}  // namespace

int main() {
    Widget::destroyed = 0;

    {
        nopeui::Ref<Widget> empty;
        assert(!empty);

        nopeui::Ref<Widget> first(new Widget());
        assert(static_cast<bool>(first));
        first->value = 7;

        nopeui::Ref<Widget> second = first;
        assert(second->value == 7);
        second.Reset();
        assert(Widget::destroyed == 0);

        nopeui::Ref<DerivedWidget> derived(new DerivedWidget());
        nopeui::Ref<Widget> base = derived;
        assert(static_cast<bool>(base));
        assert(Widget::destroyed == 0);
    }

    assert(Widget::destroyed == 2);
    return 0;
}
