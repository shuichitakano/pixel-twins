#include "pixel_twins/framebuffer.hpp"

#include <utility>

namespace pixel_twins {

Framebuffer::Framebuffer() noexcept {
    palette_[kTransparentColor] = Rgb{0, 0, 0};
    palette_[kDrawableBlackColor] = Rgb{0, 0, 0};
    palette_[kWhiteColor] = Rgb{255, 255, 255};
}

PixelBuffer& Framebuffer::drawBuffer() noexcept {
    return buffers_[drawIndex_];
}

const PixelBuffer& Framebuffer::drawBuffer() const noexcept {
    return buffers_[drawIndex_];
}

const PixelBuffer& Framebuffer::displayBuffer() const noexcept {
    return buffers_[displayIndex_];
}

const Palette& Framebuffer::palette() const noexcept {
    return palette_;
}

bool Framebuffer::setPaletteColor(ColorIndex index, Rgb color) noexcept {
    if (index == kTransparentColor || index == kDrawableBlackColor || index == kWhiteColor) {
        return false;
    }
    palette_[index] = color;
    return true;
}

void Framebuffer::flip() noexcept {
    std::swap(drawIndex_, displayIndex_);
}

} // namespace pixel_twins
