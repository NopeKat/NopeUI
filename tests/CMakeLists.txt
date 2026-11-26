function(add_nopeui_base_test test_name source_file)
    set(target "nopeui_base_${test_name}")
    add_executable(${target} ${source_file})
    target_link_libraries(${target} PRIVATE nopeui_base)
    add_test(NAME "nopeui_base.${test_name}" COMMAND ${target})
endfunction()

add_nopeui_base_test(event_test base/event_test.cpp)
add_nopeui_base_test(geometry_test base/geometry_test.cpp)
add_nopeui_base_test(ref_test base/ref_test.cpp)
add_nopeui_base_test(result_test base/result_test.cpp)
add_nopeui_base_test(strings_test base/strings_test.cpp)
add_nopeui_base_test(version_test base/version_test.cpp)

add_executable(nopeui_platform_window_test platform/window_test.cpp)
target_link_libraries(nopeui_platform_window_test PRIVATE nopeui_platform)
add_test(NAME nopeui_platform.window_test COMMAND nopeui_platform_window_test)

add_executable(nopeui_platform_window_async_test platform/window_async_test.cpp)
target_link_libraries(nopeui_platform_window_async_test PRIVATE nopeui_platform)
add_test(NAME nopeui_platform.window_async_test COMMAND nopeui_platform_window_async_test)

add_executable(nopeui_platform_popup_test platform/popup_test.cpp)
target_link_libraries(nopeui_platform_popup_test PRIVATE nopeui_platform nopeui_controls)
add_test(NAME nopeui_platform.popup_test COMMAND nopeui_platform_popup_test)

add_executable(nopeui_layout_property_style_test layout/property_style_test.cpp)
target_link_libraries(nopeui_layout_property_style_test PRIVATE nopeui_controls nopeui_layout)
add_test(NAME nopeui_layout.property_style_test COMMAND nopeui_layout_property_style_test)

add_executable(nopeui_markup_xaml_test markup/xaml_test.cpp)
target_link_libraries(nopeui_markup_xaml_test PRIVATE nopeui_markup nopeui_controls)
add_test(NAME nopeui_markup.xaml_test COMMAND nopeui_markup_xaml_test)
