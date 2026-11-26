#include "../test_check.h"

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
        NOPEUI_CHECK(!empty);

        nopeui::Ref<Widget> first(new Widget());
        NOPEUI_CHECK(static_cast<bool>(first));
        first->value = 7;

        nopeui::Ref<Widget> second = first;
        NOPEUI_CHECK(second->value == 7);
        second.Reset();
        NOPEUI_CHECK(Widget::destroyed == 0);

        nopeui::Ref<DerivedWidget> derived(new DerivedWidget());
        nopeui::Ref<Widget> base = derived;
        NOPEUI_CHECK(static_cast<bool>(base));
        NOPEUI_CHECK(Widget::destroyed == 0);
    }

    NOPEUI_CHECK(Widget::destroyed == 2);
    return 0;
}
