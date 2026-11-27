add_executable(nopeui_golden golden/main.cpp)
target_include_directories(nopeui_golden PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../samples/common)
target_link_libraries(nopeui_golden PRIVATE nopeui_graphics nopeui_controls nopeui_layout nopeui_theme nopeui_markup)

add_executable(nopeui_image_diff imagediff/main.cpp)
target_link_libraries(nopeui_image_diff PRIVATE nopeui_graphics)
