#include "pixel_twins/font.hpp"

#include <cstddef>
#include <cstdint>

namespace pixel_twins {
namespace {

[[nodiscard]] PIXEL_TWINS_FORCE_INLINE const Glyph* findGlyph(const BitmapFont& font,
                                                               std::uint8_t codepoint) noexcept {
    if (font.glyphs == nullptr || font.glyphCount == 0) {
        return nullptr;
    }
    const auto first = static_cast<std::uint16_t>(font.firstCodepoint);
    const auto end = static_cast<std::uint16_t>(first + font.glyphCount);
    auto selected = static_cast<std::uint16_t>(codepoint);
    if (selected < first || selected >= end) {
        selected = font.fallbackCodepoint;
        if (selected < first || selected >= end) {
            return nullptr;
        }
    }
    return font.glyphs + (selected - first);
}

PIXEL_TWINS_FORCE_INLINE void drawGlyphPixel(ColorIndex* destination,
                                             std::uint8_t mask,
                                             std::uint8_t outline,
                                             std::uint8_t body,
                                             ColorIndex outlineColor,
                                             ColorIndex bodyColor) noexcept {
    if ((body & mask) != 0) {
        *destination = bodyColor;
    } else if ((outline & mask) != 0) {
        *destination = outlineColor;
    }
}

PIXEL_TWINS_FORCE_INLINE void drawGlyphRow(ColorIndex* destination,
                                           const GlyphRow& row,
                                           ColorIndex outlineColor,
                                           ColorIndex bodyColor) noexcept {
    drawGlyphPixel(destination, 0x80, row.outline, row.body, outlineColor, bodyColor);
    drawGlyphPixel(destination + 1, 0x40, row.outline, row.body, outlineColor, bodyColor);
    drawGlyphPixel(destination + 2, 0x20, row.outline, row.body, outlineColor, bodyColor);
    drawGlyphPixel(destination + 3, 0x10, row.outline, row.body, outlineColor, bodyColor);
    drawGlyphPixel(destination + 4, 0x08, row.outline, row.body, outlineColor, bodyColor);
    drawGlyphPixel(destination + 5, 0x04, row.outline, row.body, outlineColor, bodyColor);
    drawGlyphPixel(destination + 6, 0x02, row.outline, row.body, outlineColor, bodyColor);
    drawGlyphPixel(destination + 7, 0x01, row.outline, row.body, outlineColor, bodyColor);
}

void drawGlyphUnclipped(RenderTarget target,
                        const Glyph& glyph,
                        std::int32_t x,
                        std::int32_t y,
                        ColorIndex outlineColor,
                        ColorIndex bodyColor) noexcept PIXEL_TWINS_SRAM;
void drawGlyphUnclipped(RenderTarget target,
                        const Glyph& glyph,
                        std::int32_t x,
                        std::int32_t y,
                        ColorIndex outlineColor,
                        ColorIndex bodyColor) noexcept {
    auto* destination = target.pixels
        + static_cast<std::size_t>(target.originY + y) * target.stride
        + static_cast<std::size_t>(target.originX + x);
    for (const auto& row : glyph.rows) {
        drawGlyphRow(destination, row, outlineColor, bodyColor);
        destination += target.stride;
    }
}

void drawGlyphClipped(RenderTarget target,
                      const Glyph& glyph,
                      std::int32_t x,
                      std::int32_t y,
                      ColorIndex outlineColor,
                      ColorIndex bodyColor) noexcept PIXEL_TWINS_SRAM;
void drawGlyphClipped(RenderTarget target,
                      const Glyph& glyph,
                      std::int32_t x,
                      std::int32_t y,
                      ColorIndex outlineColor,
                      ColorIndex bodyColor) noexcept {
    const auto startX = x < 0 ? static_cast<std::int32_t>(-x) : 0;
    const auto startY = y < 0 ? static_cast<std::int32_t>(-y) : 0;
    const auto endX = static_cast<std::int32_t>(target.width) - x < kGlyphWidth
        ? static_cast<std::int32_t>(target.width) - x
        : kGlyphWidth;
    const auto endY = static_cast<std::int32_t>(target.height) - y < kGlyphHeight
        ? static_cast<std::int32_t>(target.height) - y
        : kGlyphHeight;
    if (startX >= endX || startY >= endY) {
        return;
    }

    for (auto glyphY = startY; glyphY < endY; ++glyphY) {
        auto* destination = target.pixels
            + static_cast<std::size_t>(target.originY + y + glyphY) * target.stride
            + static_cast<std::size_t>(target.originX + x + startX);
        const auto& row = glyph.rows[static_cast<std::size_t>(glyphY)];
        auto mask = static_cast<std::uint8_t>(0x80U >> startX);
        for (auto glyphX = startX; glyphX < endX; ++glyphX) {
            drawGlyphPixel(destination++, mask, row.outline, row.body, outlineColor, bodyColor);
            mask = static_cast<std::uint8_t>(mask >> 1U);
        }
    }
}

void drawGlyphAt(RenderTarget target,
                 const BitmapFont& font,
                 std::int32_t x,
                 std::int32_t y,
                 std::uint8_t codepoint,
                 ColorIndex bodyColor) noexcept PIXEL_TWINS_SRAM;
void drawGlyphAt(RenderTarget target,
                 const BitmapFont& font,
                 std::int32_t x,
                 std::int32_t y,
                 std::uint8_t codepoint,
                 ColorIndex bodyColor) noexcept {
    const auto* glyph = findGlyph(font, codepoint);
    if (glyph == nullptr) {
        return;
    }
    const auto inside = x >= 0 && y >= 0
        && x + kGlyphWidth <= target.width && y + kGlyphHeight <= target.height;
    if (inside) {
        drawGlyphUnclipped(target, *glyph, x, y, font.outlineColor, bodyColor);
    } else {
        drawGlyphClipped(target, *glyph, x, y, font.outlineColor, bodyColor);
    }
}

} // namespace

void drawGlyph(RenderTarget target,
               const BitmapFont& font,
               std::int16_t x,
               std::int16_t y,
               std::uint8_t codepoint,
               ColorIndex bodyColor) noexcept {
    drawGlyphAt(target, font, x, y, codepoint, bodyColor);
}

void drawText(RenderTarget target,
              const BitmapFont& font,
              std::int16_t x,
              std::int16_t y,
              std::string_view text,
              ColorIndex bodyColor,
              std::int16_t stride) noexcept {
    const auto lineStart = static_cast<std::int32_t>(x);
    auto destinationX = lineStart;
    auto destinationY = static_cast<std::int32_t>(y);
    for (const auto character : text) {
        if (character == '\n') {
            destinationX = lineStart;
            destinationY += kGlyphHeight;
            continue;
        }
        if (character == '\r') {
            continue;
        }
        drawGlyphAt(target,
                    font,
                    destinationX,
                    destinationY,
                    static_cast<std::uint8_t>(character),
                    bodyColor);
        destinationX += stride;
    }
}

} // namespace pixel_twins
