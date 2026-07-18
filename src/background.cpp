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

void fillRows(RenderTarget target,
              std::uint16_t firstY,
              std::uint16_t endY,
              ColorIndex color) noexcept PIXEL_TWINS_SRAM;
void fillRows(RenderTarget target,
              std::uint16_t firstY,
              std::uint16_t endY,
              ColorIndex color) noexcept {
    for (auto y = firstY; y < endY; ++y) {
        auto* destination = target.pixels
            + static_cast<std::size_t>(target.originY + y) * target.stride + target.originX;
        fillRow(destination, target.width, color);
    }
}

struct AxisRange {
    std::uint16_t destinationStart;
    std::uint16_t destinationEnd;
    std::uint32_t sourceStart;
};

[[nodiscard]] PIXEL_TWINS_FORCE_INLINE bool isValidTileSize(std::uint8_t size) noexcept {
    return size >= 8 && (size & static_cast<std::uint8_t>(size - 1)) == 0;
}

[[nodiscard]] PIXEL_TWINS_FORCE_INLINE std::uint8_t tileShift(std::uint8_t size) noexcept {
    std::uint8_t shift = 0;
    while (size > 1) {
        size = static_cast<std::uint8_t>(size >> 1U);
        ++shift;
    }
    return shift;
}

[[nodiscard]] PIXEL_TWINS_FORCE_INLINE AxisRange clipAxis(std::int32_t source,
                                                           std::uint16_t destinationSize,
                                                           std::uint32_t sourceSize) noexcept {
    const auto source64 = static_cast<std::int64_t>(source);
    const auto sourceSize64 = static_cast<std::int64_t>(sourceSize);
    const auto destinationSize64 = static_cast<std::int64_t>(destinationSize);
    const auto destinationStart64 = std::clamp(-source64, std::int64_t{0}, destinationSize64);
    const auto firstSource64 = source64 + destinationStart64;
    const auto available64 = std::max<std::int64_t>(0, sourceSize64 - firstSource64);
    const auto count64 = std::min(destinationSize64 - destinationStart64, available64);

    return AxisRange{static_cast<std::uint16_t>(destinationStart64),
                     static_cast<std::uint16_t>(destinationStart64 + count64),
                     static_cast<std::uint32_t>(std::max<std::int64_t>(0, firstSource64))};
}

} // namespace

void drawBackground(RenderTarget target,
                    const Background& background,
                    std::int32_t sourceX,
                    std::int32_t sourceY) noexcept {
    if (target.pixels == nullptr || target.width == 0 || target.height == 0) {
        return;
    }

    const auto validTileSize = isValidTileSize(background.tileWidth)
        && isValidTileSize(background.tileHeight);
    if (!validTileSize || background.tilemap == nullptr || background.patterns == nullptr) {
        return;
    }
    if (background.width == 0 || background.height == 0) {
        clear(target, kTransparentColor);
        return;
    }

    const auto virtualWidth = static_cast<std::uint32_t>(background.width)
        * background.tileWidth;
    const auto virtualHeight = static_cast<std::uint32_t>(background.height)
        * background.tileHeight;
    const auto horizontal = clipAxis(sourceX, target.width, virtualWidth);
    const auto vertical = clipAxis(sourceY, target.height, virtualHeight);

    fillRows(target, 0, vertical.destinationStart, kTransparentColor);
    fillRows(target, vertical.destinationEnd, target.height, kTransparentColor);
    if (horizontal.destinationStart == horizontal.destinationEnd
        || vertical.destinationStart == vertical.destinationEnd) {
        fillRows(target,
                 vertical.destinationStart,
                 vertical.destinationEnd,
                 kTransparentColor);
        return;
    }

    const auto tileWidthShift = tileShift(background.tileWidth);
    const auto tileHeightShift = tileShift(background.tileHeight);
    const auto tileWidthMask = static_cast<std::uint32_t>(background.tileWidth - 1);
    const auto tileHeightMask = static_cast<std::uint32_t>(background.tileHeight - 1);
    const auto firstTileX = horizontal.sourceStart >> tileWidthShift;
    const auto firstPixelX = horizontal.sourceStart & tileWidthMask;
    auto tileY = vertical.sourceStart >> tileHeightShift;
    auto pixelY = vertical.sourceStart & tileHeightMask;
    const auto tilePixelCount = static_cast<std::size_t>(background.tileWidth)
        * background.tileHeight;

    for (auto destinationY = vertical.destinationStart;
         destinationY < vertical.destinationEnd;
         ++destinationY) {
        auto* destination = target.pixels
            + static_cast<std::size_t>(target.originY + destinationY) * target.stride
            + target.originX;
        fillRow(destination, horizontal.destinationStart, kTransparentColor);

        auto destinationX = horizontal.destinationStart;
        auto tileX = firstTileX;
        auto pixelX = firstPixelX;
        while (destinationX < horizontal.destinationEnd) {
            const auto tileIndex = background.tilemap[
                static_cast<std::size_t>(tileY) * background.width + tileX];
            const auto* source = background.patterns
                + static_cast<std::size_t>(tileIndex) * tilePixelCount
                + static_cast<std::size_t>(pixelY) * background.tileWidth + pixelX;
            auto segmentWidth = std::min<std::uint32_t>(
                background.tileWidth - pixelX,
                static_cast<std::uint32_t>(horizontal.destinationEnd - destinationX));

            while (segmentWidth >= 8) {
                copyEight(destination + destinationX, source);
                destinationX = static_cast<std::uint16_t>(destinationX + 8);
                source += 8;
                segmentWidth -= 8;
                pixelX += 8;
            }
            while (segmentWidth > 0) {
                destination[destinationX++] = *source++;
                --segmentWidth;
                ++pixelX;
            }
            if (pixelX == background.tileWidth) {
                pixelX = 0;
                ++tileX;
            }
        }

        fillRow(destination + horizontal.destinationEnd,
                static_cast<std::uint16_t>(target.width - horizontal.destinationEnd),
                kTransparentColor);

        ++pixelY;
        if (pixelY == background.tileHeight) {
            pixelY = 0;
            ++tileY;
        }
    }
}

} // namespace pixel_twins
