#include "pixel_twins/sprite.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace pixel_twins {
namespace {

PIXEL_TWINS_FORCE_INLINE void copyOpaque(ColorIndex* destination, ColorIndex source) noexcept {
    if (source != kTransparentColor) {
        *destination = source;
    }
}

template<std::size_t... Index>
PIXEL_TWINS_FORCE_INLINE void copyFixedRow(ColorIndex* destination,
                                           const ColorIndex* source,
                                           std::index_sequence<Index...>) noexcept {
    (copyOpaque(destination + Index, source[Index]), ...);
}

template<std::size_t Width>
PIXEL_TWINS_FORCE_INLINE void drawFixedWidth(RenderTarget target,
                                             const Sprite& sprite) noexcept {
    auto* destination = target.pixels
        + static_cast<std::size_t>(target.originY + sprite.dy) * target.stride
        + static_cast<std::size_t>(target.originX + sprite.dx);
    auto* source = sprite.p;

    for (std::uint8_t y = 0; y < sprite.sh; ++y) {
        copyFixedRow(destination, source, std::make_index_sequence<Width>{});
        destination += target.stride;
        source += Width;
    }
}

PIXEL_TWINS_FORCE_INLINE void copyEight(ColorIndex* destination,
                                        const ColorIndex* source) noexcept {
    copyFixedRow(destination, source, std::make_index_sequence<8>{});
}

void drawGenericUnclipped(RenderTarget target, const Sprite& sprite) noexcept PIXEL_TWINS_SRAM;
void drawGenericUnclipped(RenderTarget target, const Sprite& sprite) noexcept {
    auto* destination = target.pixels
        + static_cast<std::size_t>(target.originY + sprite.dy) * target.stride
        + static_cast<std::size_t>(target.originX + sprite.dx);
    auto* source = sprite.p;

    for (std::uint8_t y = 0; y < sprite.sh; ++y) {
        std::uint8_t x = 0;
        for (; static_cast<unsigned>(x) + 8U <= sprite.sw; x = static_cast<std::uint8_t>(x + 8)) {
            copyEight(destination + x, source + x);
        }
        for (; x < sprite.sw; ++x) {
            copyOpaque(destination + x, source[x]);
        }
        destination += target.stride;
        source += sprite.sw;
    }
}

void drawClipped(RenderTarget target, const Sprite& sprite) noexcept PIXEL_TWINS_SRAM;
void drawClipped(RenderTarget target, const Sprite& sprite) noexcept {
    const auto startX = std::max<std::int32_t>(0, -sprite.dx);
    const auto startY = std::max<std::int32_t>(0, -sprite.dy);
    const auto endX = std::min<std::int32_t>(sprite.sw,
                                             static_cast<std::int32_t>(target.width) - sprite.dx);
    const auto endY = std::min<std::int32_t>(sprite.sh,
                                             static_cast<std::int32_t>(target.height) - sprite.dy);
    if (startX >= endX || startY >= endY) {
        return;
    }

    for (auto y = startY; y < endY; ++y) {
        auto* destination = target.pixels
            + static_cast<std::size_t>(target.originY + sprite.dy + y) * target.stride
            + static_cast<std::size_t>(target.originX + sprite.dx + startX);
        const auto* source = sprite.p + static_cast<std::size_t>(y) * sprite.sw + startX;
        for (auto x = startX; x < endX; ++x) {
            copyOpaque(destination++, *source++);
        }
    }
}

[[nodiscard]] PIXEL_TWINS_FORCE_INLINE bool isInside(RenderTarget target,
                                                      const Sprite& sprite) noexcept {
    return sprite.dx >= 0 && sprite.dy >= 0
        && static_cast<std::int32_t>(sprite.dx) + sprite.sw <= target.width
        && static_cast<std::int32_t>(sprite.dy) + sprite.sh <= target.height;
}

} // namespace

void drawSprite(RenderTarget target, const Sprite& sprite) noexcept {
    if (sprite.p == nullptr || sprite.sw == 0 || sprite.sh == 0) {
        return;
    }
    if (!isInside(target, sprite)) {
        drawClipped(target, sprite);
        return;
    }

    switch (sprite.sw) {
    case 8: drawFixedWidth<8>(target, sprite); return;
    case 16: drawFixedWidth<16>(target, sprite); return;
    case 17: drawFixedWidth<17>(target, sprite); return;
    case 18: drawFixedWidth<18>(target, sprite); return;
    case 19: drawFixedWidth<19>(target, sprite); return;
    case 20: drawFixedWidth<20>(target, sprite); return;
    case 21: drawFixedWidth<21>(target, sprite); return;
    case 22: drawFixedWidth<22>(target, sprite); return;
    case 23: drawFixedWidth<23>(target, sprite); return;
    case 24: drawFixedWidth<24>(target, sprite); return;
    default: drawGenericUnclipped(target, sprite); return;
    }
}

void drawSpriteEx(RenderTarget target, const SpriteEx& sprite) noexcept {
    if (sprite.p == nullptr || sprite.dw == 0 || sprite.dh == 0 || sprite.sw == 0
        || sprite.sh == 0) {
        return;
    }

    const auto startX = std::max<std::int32_t>(0, -sprite.dx);
    const auto startY = std::max<std::int32_t>(0, -sprite.dy);
    const auto endX = std::min<std::int32_t>(sprite.dw,
                                             static_cast<std::int32_t>(target.width) - sprite.dx);
    const auto endY = std::min<std::int32_t>(sprite.dh,
                                             static_cast<std::int32_t>(target.height) - sprite.dy);
    if (startX >= endX || startY >= endY) {
        return;
    }

    for (auto y = startY; y < endY; ++y) {
        const auto sourceY = (static_cast<std::uint32_t>(y) * sprite.sh + sprite.sh / 2U)
            / sprite.dh;
        auto* destination = target.pixels
            + static_cast<std::size_t>(target.originY + sprite.dy + y) * target.stride
            + static_cast<std::size_t>(target.originX + sprite.dx + startX);
        for (auto x = startX; x < endX; ++x) {
            const auto sourceX = (static_cast<std::uint32_t>(x) * sprite.sw + sprite.sw / 2U)
                / sprite.dw;
            const auto source = sprite.p[sourceY * sprite.sw + sourceX];
            copyOpaque(destination++, source);
        }
    }
}

} // namespace pixel_twins
