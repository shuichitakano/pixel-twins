#include "pixel_twins/background.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace pixel_twins {
namespace {

PIXEL_TWINS_FORCE_INLINE void copyEight(ColorIndex* destination,
                                        const ColorIndex* source) noexcept {
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = source[3];
    destination[4] = source[4];
    destination[5] = source[5];
    destination[6] = source[6];
    destination[7] = source[7];
}

PIXEL_TWINS_FORCE_INLINE void fillEight(ColorIndex* destination,
                                        ColorIndex color) noexcept {
    destination[0] = color;
    destination[1] = color;
    destination[2] = color;
    destination[3] = color;
    destination[4] = color;
    destination[5] = color;
    destination[6] = color;
    destination[7] = color;
}

void fillRow(ColorIndex* destination, std::uint16_t width, ColorIndex color) noexcept
    PIXEL_TWINS_SRAM;
void fillRow(ColorIndex* destination, std::uint16_t width, ColorIndex color) noexcept {
    std::uint16_t x = 0;
    for (; static_cast<unsigned>(x) + 8U <= width; x = static_cast<std::uint16_t>(x + 8)) {
        fillEight(destination + x, color);
    }
    for (; x < width; ++x) {
        destination[x] = color;
    }
}

} // namespace

void drawBackground(RenderTarget target,
                    const Background& background,
                    std::int32_t sourceX,
                    std::int32_t sourceY) noexcept {
    if (target.pixels == nullptr || target.width == 0 || target.height == 0) {
        return;
    }

    const auto validTileSize = background.tileWidth != 0 && background.tileHeight != 0
        && background.tileWidth % 8 == 0 && background.tileHeight % 8 == 0;
    if (!validTileSize || background.tilemap == nullptr || background.patterns == nullptr) {
        return;
    }
    if (background.width == 0 || background.height == 0) {
        clear(target, kTransparentColor);
        return;
    }

    const auto virtualWidth = static_cast<std::int32_t>(background.width) * background.tileWidth;
    const auto virtualHeight = static_cast<std::int32_t>(background.height) * background.tileHeight;
    const auto tilePixelCount = static_cast<std::size_t>(background.tileWidth)
        * background.tileHeight;

    for (std::uint16_t destinationY = 0; destinationY < target.height; ++destinationY) {
        auto* destination = target.pixels
            + static_cast<std::size_t>(target.originY + destinationY) * target.stride
            + target.originX;
        const auto virtualY = sourceY + destinationY;
        if (virtualY < 0 || virtualY >= virtualHeight) {
            fillRow(destination, target.width, kTransparentColor);
            continue;
        }

        const auto tileY = virtualY / background.tileHeight;
        const auto pixelY = virtualY % background.tileHeight;
        std::int32_t destinationX = 0;
        auto virtualX = sourceX;

        if (virtualX < 0) {
            const auto outside = std::min<std::int32_t>(target.width, -virtualX);
            fillRow(destination, static_cast<std::uint16_t>(outside), kTransparentColor);
            destinationX += outside;
            virtualX += outside;
        }

        const auto validEnd = std::min<std::int32_t>(
            target.width,
            destinationX + std::max<std::int32_t>(0, virtualWidth - virtualX));

        while (destinationX < validEnd) {
            const auto tileX = virtualX / background.tileWidth;
            const auto pixelX = virtualX % background.tileWidth;
            const auto tileIndex = background.tilemap[
                static_cast<std::size_t>(tileY) * background.width
                + static_cast<std::size_t>(tileX)];
            const auto* source = background.patterns
                + static_cast<std::size_t>(tileIndex) * tilePixelCount
                + static_cast<std::size_t>(pixelY) * background.tileWidth + pixelX;
            auto segmentWidth = std::min<std::int32_t>(background.tileWidth - pixelX,
                                                       validEnd - destinationX);

            while (segmentWidth >= 8) {
                copyEight(destination + destinationX, source);
                destinationX += 8;
                virtualX += 8;
                source += 8;
                segmentWidth -= 8;
            }
            while (segmentWidth > 0) {
                destination[destinationX++] = *source++;
                ++virtualX;
                --segmentWidth;
            }
        }

        if (destinationX < target.width) {
            fillRow(destination + destinationX,
                    static_cast<std::uint16_t>(target.width - destinationX),
                    kTransparentColor);
        }
    }
}

} // namespace pixel_twins
