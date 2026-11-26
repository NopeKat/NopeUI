#ifndef NOPEUI_GRAPHICS_HARDWARE_H_
#define NOPEUI_GRAPHICS_HARDWARE_H_

#include <cstdint>
#include <memory>
#include <string_view>

#include "nopeui/base/result.h"

struct ID2D1RenderTarget;

namespace nopeui::graphics {

// A GPU Direct2D target backed by a D3D11 texture, with a staging readback for
// golden image comparison against the software path.
class HardwareSurface {
public:
    static Result<std::unique_ptr<HardwareSurface>> Create(
        int width,
        int height,
        float dpi = 96.0f,
        bool allow_warp = true) noexcept;

    ~HardwareSurface();

    HardwareSurface(const HardwareSurface&) = delete;
    HardwareSurface& operator=(const HardwareSurface&) = delete;

    ID2D1RenderTarget* Target() const noexcept;
    int Width() const noexcept;
    int Height() const noexcept;
    bool IsWarp() const noexcept;

    Result<void> BeginDraw() noexcept;
    Result<void> EndDraw() noexcept;
    Result<void> SavePng(std::string_view utf8_path) noexcept;

private:
    HardwareSurface() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nopeui::graphics

#endif  // NOPEUI_GRAPHICS_HARDWARE_H_
