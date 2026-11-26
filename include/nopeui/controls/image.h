#ifndef NOPEUI_CONTROLS_IMAGE_H_
#define NOPEUI_CONTROLS_IMAGE_H_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "nopeui/base/ref.h"
#include "nopeui/graphics/image.h"
#include "nopeui/layout/element.h"

namespace nopeui {

enum class ImageStretch {
    None,
    Fill,
    Uniform,
    UniformToFill,
};

// Bitmap control. The source stays in straight-alpha BGRA; the D2D bitmap
// cache premultiplies once per render target.
class Image : public Element {
public:
    static Ref<Image> Create() noexcept;
    static Result<Ref<Image>> CreateFromFile(std::string_view utf8_path) noexcept;

    ~Image() override;

    void SetImage(graphics::Image image) noexcept;
    const graphics::Image& Source() const noexcept {
        return image_;
    }

    bool LoadFromFile(std::string_view utf8_path) noexcept;

    void SetStretch(ImageStretch stretch) noexcept;
    ImageStretch Stretch() const noexcept {
        return stretch_;
    }

    // Zero on either axis keeps the natural pixel size for that axis.
    void SetDisplaySize(Size size) noexcept;
    Size DisplaySize() const noexcept {
        return display_size_;
    }

    std::wstring_view TypeName() const noexcept override {
        return L"Image";
    }

protected:
    Size MeasureOverride(Size available) noexcept override;
    void PaintContent(ID2D1RenderTarget* target) noexcept override;

    const PropertyMetadata* ControlPropertyTable(std::size_t* count) const noexcept override;
    bool GetControlProperty(std::wstring_view name, PropertyValue* value) const noexcept override;
    bool SetControlProperty(std::wstring_view name, const PropertyValue& value) noexcept override;

private:
    Image() noexcept = default;

    struct BitmapCache;
    std::unique_ptr<BitmapCache> cache_;
    graphics::Image image_;
    ImageStretch stretch_ = ImageStretch::Uniform;
    Size display_size_{};
};

}  // namespace nopeui

#endif  // NOPEUI_CONTROLS_IMAGE_H_
