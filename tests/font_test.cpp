#include "pixel_twins/font.hpp"
#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/render_target.hpp"
#include "test_check.hpp"

#include <array>
#include <cstddef>

namespace {

using namespace pixel_twins;

constexpr Glyph makeFallbackGlyph() {
    Glyph glyph{};
    glyph.rows[0] = GlyphRow{0x40, 0x80};
    return glyph;
}

constexpr Glyph makeAtGlyph() {
    Glyph glyph{};
    glyph.rows[0] = GlyphRow{0x00, 0x20};
    return glyph;
}

constexpr Glyph makeAGlyph() {
    Glyph glyph{};
    glyph.rows[0] = GlyphRow{0x80, 0x40};
    glyph.rows[1] = GlyphRow{0x00, 0x20};
    return glyph;
}

constexpr std::array<Glyph, 3> kGlyphs{makeFallbackGlyph(), makeAtGlyph(), makeAGlyph()};
constexpr BitmapFont kFont{kGlyphs.data(), 63, 3, 63, 1};

ColorIndex pixelAt(const PixelBuffer& buffer, std::size_t x, std::size_t y) {
    return buffer[y * kScreenWidth + x];
}

void testGlyphColorsAndFallback() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    clear(target, 0);

    drawGlyph(target, kFont, 4, 3, 'A', 7);
    check(pixelAt(buffer, 4, 3) == 1);
    check(pixelAt(buffer, 5, 3) == 7);
    check(pixelAt(buffer, 6, 4) == 7);
    check(pixelAt(buffer, 7, 3) == 0);

    drawGlyph(target, kFont, 12, 3, 'Z', 6);
    check(pixelAt(buffer, 12, 3) == 6);
    check(pixelAt(buffer, 13, 3) == 1);
}

void testStrideAndNewline() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    clear(target, 0);

    drawText(target, kFont, 10, 2, "AA\nA", 5, 3);
    check(pixelAt(buffer, 11, 2) == 5);
    check(pixelAt(buffer, 14, 2) == 5);
    check(pixelAt(buffer, 11, 11) == 5);

    drawText(target, kFont, 30, 20, "AA", 6, -4);
    check(pixelAt(buffer, 31, 20) == 6);
    check(pixelAt(buffer, 27, 20) == 6);
}

void testClippingAndPanelBoundary() {
    PixelBuffer buffer;
    auto left = makeRenderTarget(buffer, Screen::Left);
    auto right = makeRenderTarget(buffer, Screen::Right);
    clear(left, 0);
    clear(right, 9);

    drawGlyph(left, kFont, -1, -1, 'A', 4);
    check(pixelAt(buffer, 1, 0) == 4);
    drawGlyph(left, kFont, 158, 4, 'A', 4);
    check(pixelAt(buffer, 158, 4) == 1);
    check(pixelAt(buffer, 159, 4) == 4);
    check(pixelAt(buffer, 160, 4) == 9);
}

} // namespace

int main() {
    testGlyphColorsAndFallback();
    testStrideAndNewline();
    testClippingAndPanelBoundary();
    return 0;
}
