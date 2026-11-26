add_executable(nopeui_window_smoke WIN32
    window_smoke/main.cpp
)

target_link_libraries(nopeui_window_smoke PRIVATE nopeui_platform)

add_executable(nopeui_frame_benchmark
    frame_benchmark/main.cpp
)

target_link_libraries(nopeui_frame_benchmark PRIVATE nopeui_platform dwmapi)

add_executable(nopeui_resize_probe
    resize_probe/main.cpp
)

target_link_libraries(nopeui_resize_probe PRIVATE user32)

add_executable(nopeui_resize_benchmark
    resize_benchmark/main.cpp
)

target_link_libraries(nopeui_resize_benchmark PRIVATE nopeui_platform)
