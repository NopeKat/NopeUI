add_executable(nopeui_window_smoke WIN32
    window_smoke/main.cpp
)

target_link_libraries(nopeui_window_smoke PRIVATE nopeui_platform)
