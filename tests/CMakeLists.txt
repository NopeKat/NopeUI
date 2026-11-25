add_executable(nopeui_base_tests
    base/version_test.cpp
)

target_link_libraries(nopeui_base_tests PRIVATE nopeui_base)

add_test(NAME nopeui_base.version COMMAND nopeui_base_tests)
