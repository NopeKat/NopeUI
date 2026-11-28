add_executable(nopeui_window_smoke WIN32
    window_smoke/main.cpp
)

target_include_directories(nopeui_window_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(nopeui_window_smoke PRIVATE nopeui_platform nopeui_controls)

add_executable(nopeui_gallery WIN32
    gallery/main.cpp
)

target_link_libraries(nopeui_gallery PRIVATE nopeui_platform nopeui_controls nopeui_theme)

add_executable(nopeui_xaml_smoke WIN32
    xaml_smoke/main.cpp
)

target_link_libraries(nopeui_xaml_smoke PRIVATE nopeui_platform nopeui_markup)

add_custom_command(
    TARGET nopeui_xaml_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/xaml_smoke/page.xaml"
        "$<TARGET_FILE_DIR:nopeui_xaml_smoke>/page.xaml"
    VERBATIM
)

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

add_executable(nopeui_text_list_benchmark
    text_list_benchmark/main.cpp
)

target_link_libraries(nopeui_text_list_benchmark PRIVATE nopeui_platform)

add_executable(nopeui_present_benchmark
    present_benchmark/main.cpp
)

target_link_libraries(nopeui_present_benchmark PRIVATE d3d11 dxgi dcomp user32)

add_executable(nopeui_startup_benchmark
    startup_benchmark/main.cpp
)

target_include_directories(nopeui_startup_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(nopeui_startup_benchmark PRIVATE nopeui_platform nopeui_controls nopeui_theme psapi)

add_executable(nopeui_theme_benchmark
    theme_benchmark/main.cpp
)

target_include_directories(nopeui_theme_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(nopeui_theme_benchmark PRIVATE nopeui_platform nopeui_controls nopeui_theme psapi)
