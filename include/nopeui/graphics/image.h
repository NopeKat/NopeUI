#ifndef NOPEUI_GRAPHICS_IMAGE_H_
#define NOPEUI_GRAPHICS_IMAGE_H_

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "nopeui/base/result.h"

namespace nopeui::graphics {

// 32-bit BGRA pixels, top-down rows, straight (non-premultiplied) alpha.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;

    bool Empty() const noexcept {
        if (width <= 0 || height <= 0) {
            return true;
        }
        const std::size_t expected =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        return pixels.size() != expected;
    }
};

Result<void> SavePng(
    std::string_view utf8_path,
    int width,
    int height,
    std::span<const std::uint32_t> pixels) noexcept;

Result<Image> LoadPng(std::string_view utf8_path) noexcept;

}  // namespace nopeui::graphics

#endif  // NOPEUI_GRAPHICS_IMAGE_H_
