add_executable(nopeui_golden golden/main.cpp)
target_link_libraries(nopeui_golden PRIVATE nopeui_graphics)

add_executable(nopeui_image_diff imagediff/main.cpp)
target_link_libraries(nopeui_image_diff PRIVATE nopeui_graphics)
