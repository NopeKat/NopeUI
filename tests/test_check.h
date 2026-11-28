#ifndef NOPEUI_TESTS_TEST_CHECK_H_
#define NOPEUI_TESTS_TEST_CHECK_H_

#include <cstdio>

#define NOPEUI_CHECK(condition)                                    \
    do {                                                           \
        if (!(condition)) {                                        \
            std::fprintf(stderr, "check failed at %s:%d: %s\n",    \
                __FILE__, __LINE__, #condition);                   \
            return 1;                                              \
        }                                                          \
    } while (false)

#endif  // NOPEUI_TESTS_TEST_CHECK_H_
