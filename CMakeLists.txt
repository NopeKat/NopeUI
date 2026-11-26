cmake_minimum_required(VERSION 3.28)

project(NopeUI VERSION 0.0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_library(nopeui_base STATIC
    src/base/strings.cpp
    src/base/version.cpp
)

target_include_directories(nopeui_base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(nopeui_base PUBLIC cxx_std_20)

add_library(nopeui_platform STATIC
    src/platform/window.cpp
)

target_link_libraries(nopeui_platform
    PUBLIC nopeui_base
    PRIVATE user32 d3d11 dxgi d2d1 dcomp dwrite dwmapi
)
target_compile_features(nopeui_platform PUBLIC cxx_std_20)

add_library(nopeui_graphics STATIC
    src/graphics/image.cpp
    src/graphics/offscreen.cpp
    src/graphics/scene.cpp
)

target_link_libraries(nopeui_graphics
    PUBLIC nopeui_base
    PRIVATE d2d1 dwrite windowscodecs ole32
)
target_compile_features(nopeui_graphics PUBLIC cxx_std_20)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_subdirectory(samples)
add_subdirectory(tools)
