#include "pixel_twins/render_target.hpp"

#include <algorithm>
#include <cstddef>

namespace pixel_twins {

RenderTarget makeRenderTarget(PixelBuffer& buffer, Screen screen) noexcept {
    switch (screen) {
    case Screen::Left:
        return RenderTarget{buffer.data(), 0, 0, kPanelWidth, kScreenHeight, kScreenWidth};
    case Screen::Right:
        return RenderTarget{buffer.data(), kPanelWidth, 0, kPanelWidth, kScreenHeight, kScreenWidth};
    case Screen::Full:
    default:
        return RenderTarget{buffer.data(), 0, 0, kScreenWidth, kScreenHeight, kScreenWidth};
    }
}

void clear(RenderTarget target, ColorIndex color) noexcept {
    for (std::uint16_t y = 0; y < target.height; ++y) {
        auto* row = target.pixels
            + static_cast<std::size_t>(target.originY + y) * target.stride
            + target.originX;
        std::fill_n(row, target.width, color);
    }
}

bool putPixel(RenderTarget target,
              std::int16_t x,
              std::int16_t y,
              ColorIndex color) noexcept {
    if (x < 0 || y < 0 || x >= target.width || y >= target.height) {
        return false;
    }

    const auto offset = static_cast<std::size_t>(target.originY + y) * target.stride
        + static_cast<std::size_t>(target.originX + x);
    target.pixels[offset] = color;
    return true;
}

} // namespace pixel_twins
