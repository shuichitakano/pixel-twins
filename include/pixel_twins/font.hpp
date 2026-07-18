#pragma once

#include "pixel_twins/platform.hpp"
#include "pixel_twins/render_target.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace pixel_twins {

inline constexpr std::uint8_t kGlyphWidth = 8;
inline constexpr std::uint8_t kGlyphHeight = 9;

struct GlyphRow {
    std::uint8_t outline;
    std::uint8_t body;
};

struct Glyph {
    std::array<GlyphRow, kGlyphHeight> rows;
};

static_assert(sizeof(GlyphRow) == 2, "GlyphRowは2バイトでなければなりません");
static_assert(sizeof(Glyph) == 18, "8×9グリフは18バイトでなければなりません");

struct BitmapFont {
    const Glyph* glyphs;
    std::uint8_t firstCodepoint;
    std::uint8_t glyphCount;
    std::uint8_t fallbackCodepoint;
    ColorIndex outlineColor;
};

void drawGlyph(RenderTarget target,
               const BitmapFont& font,
               std::int16_t x,
               std::int16_t y,
               std::uint8_t codepoint,
               ColorIndex bodyColor) noexcept PIXEL_TWINS_SRAM;

void drawText(RenderTarget target,
              const BitmapFont& font,
              std::int16_t x,
              std::int16_t y,
              std::string_view text,
              ColorIndex bodyColor,
              std::int16_t stride = kGlyphWidth) noexcept PIXEL_TWINS_SRAM;

} // namespace pixel_twins
