#pragma once

#include "pixel_twins/framebuffer.hpp"

#include <cstdint>

namespace pixel_twins {

struct RenderTarget {
    ColorIndex* pixels;
    std::int16_t originX;
    std::int16_t originY;
    std::uint16_t width;
    std::uint16_t height;
    std::uint16_t stride;
};

enum class Screen : std::uint8_t {
    Full,
    Left,
    Right,
};

[[nodiscard]] RenderTarget makeRenderTarget(PixelBuffer& buffer, Screen screen) noexcept;

void clear(RenderTarget target, ColorIndex color) noexcept PIXEL_TWINS_SRAM;
[[nodiscard]] bool putPixel(RenderTarget target,
                            std::int16_t x,
                            std::int16_t y,
                            ColorIndex color) noexcept PIXEL_TWINS_SRAM;

} // namespace pixel_twins
