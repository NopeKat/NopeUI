#ifndef NOPEUI_GRAPHICS_OFFSCREEN_H_
#define NOPEUI_GRAPHICS_OFFSCREEN_H_

#include <cstdint>
#include <memory>
#include <string_view>

#include "nopeui/base/result.h"

struct ID2D1RenderTarget;
struct IWICBitmap;

namespace nopeui::graphics {

// A WIC-backed Direct2D target for deterministic, display-independent images.
// Text antialiasing is forced to grayscale so golden images do not depend on
// the host display configuration.
class OffscreenSurface {
public:
    static Result<std::unique_ptr<OffscreenSurface>> Create(
        int width,
        int height,
        float dpi = 96.0f) noexcept;

    ~OffscreenSurface();

    OffscreenSurface(const OffscreenSurface&) = delete;
    OffscreenSurface& operator=(const OffscreenSurface&) = delete;

    ID2D1RenderTarget* Target() const noexcept;
    IWICBitmap* WicBitmap() const noexcept;
    int Width() const noexcept;
    int Height() const noexcept;

    Result<void> BeginDraw() noexcept;
    Result<void> EndDraw() noexcept;
    Result<void> SavePng(std::string_view utf8_path) const noexcept;

private:
    OffscreenSurface() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nopeui::graphics

#endif  // NOPEUI_GRAPHICS_OFFSCREEN_H_
