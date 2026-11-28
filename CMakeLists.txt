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

# Code analysis is a separate build: /analyze is slow and its findings are
# reported as warnings, so -WX is turned off to collect the whole set in one
# pass instead of stopping at the first hit.
option(NOPEUI_ANALYZE "Run the MSVC code analysis pass" OFF)
if(NOPEUI_ANALYZE AND MSVC)
    add_compile_options(/analyze /WX-)
endif()

add_library(nopeui_base STATIC
    src/base/diagnostic.cpp
    src/base/strings.cpp
    src/base/version.cpp
)

target_link_libraries(nopeui_base PRIVATE dbghelp psapi user32)

target_include_directories(nopeui_base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(nopeui_base PUBLIC cxx_std_20)

add_library(nopeui_theme STATIC
    src/theme/theme.cpp
)

target_link_libraries(nopeui_theme PUBLIC nopeui_base PRIVATE advapi32)
target_compile_features(nopeui_theme PUBLIC cxx_std_20)

add_library(nopeui_layout STATIC
    src/layout/attached.cpp
    src/layout/element.cpp
    src/layout/style.cpp
)

target_link_libraries(nopeui_layout PUBLIC nopeui_base nopeui_theme)
target_compile_features(nopeui_layout PUBLIC cxx_std_20)

add_library(nopeui_platform STATIC
    src/platform/automation.cpp
    src/platform/window.cpp
)

target_link_libraries(nopeui_platform
    PUBLIC nopeui_base nopeui_theme
    PRIVATE user32 d3d11 dxgi d2d1 dcomp dwrite dwmapi imm32 uiautomationcore oleaut32 nopeui_graphics nopeui_layout nopeui_controls
)
target_compile_features(nopeui_platform PUBLIC cxx_std_20)

add_library(nopeui_graphics STATIC
    src/graphics/hardware.cpp
    src/graphics/image.cpp
    src/graphics/offscreen.cpp
    src/graphics/scene.cpp
    src/graphics/text.cpp
)

target_link_libraries(nopeui_graphics
    PUBLIC nopeui_base
    PRIVATE d2d1 d3d11 dwrite dxgi windowscodecs ole32
)
target_compile_features(nopeui_graphics PUBLIC cxx_std_20)

add_library(nopeui_controls STATIC
    src/controls/border.cpp
    src/controls/button.cpp
    src/controls/canvas.cpp
    src/controls/check_box.cpp
    src/controls/content_presenter.cpp
    src/controls/context_menu.cpp
    src/controls/dock_panel.cpp
    src/controls/flex_panel.cpp
    src/controls/flyout.cpp
    src/controls/grid.cpp
    src/controls/image.cpp
    src/controls/label.cpp
    src/controls/menu_bar.cpp
    src/controls/progress_bar.cpp
    src/controls/radio_button.cpp
    src/controls/scroll_viewer.cpp
    src/controls/slider.cpp
    src/controls/stack_panel.cpp
    src/controls/text_box.cpp
    src/controls/toggle_button.cpp
    src/controls/toggle_switch.cpp
    src/controls/tooltip.cpp
)

target_link_libraries(nopeui_controls
    PUBLIC nopeui_base nopeui_layout
    PRIVATE d2d1 nopeui_graphics
)
target_compile_features(nopeui_controls PUBLIC cxx_std_20)

add_library(nopeui_markup STATIC
    src/markup/xaml.cpp
)

target_link_libraries(nopeui_markup
    PUBLIC nopeui_base nopeui_layout
    PRIVATE nopeui_theme nopeui_controls
)
target_compile_features(nopeui_markup PUBLIC cxx_std_20)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_subdirectory(samples)
add_subdirectory(tools)
