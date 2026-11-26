#ifndef NOPEUI_TESTS_TEST_CHECK_H_
#define NOPEUI_TESTS_TEST_CHECK_H_

#define NOPEUI_CHECK(condition) \
    do {                        \
        if (!(condition)) {     \
            return 1;           \
        }                       \
    } while (false)

#endif  // NOPEUI_TESTS_TEST_CHECK_H_
