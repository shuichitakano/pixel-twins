#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/render_target.hpp"
#include "pixel_twins/sprite.hpp"
#include "test_check.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace pixel_twins;

ColorIndex pixelAt(const PixelBuffer& buffer, std::size_t x, std::size_t y) {
    return buffer[y * kScreenWidth + x];
}

void testGenericAndTransparent() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    clear(target, 9);

    constexpr std::array<ColorIndex, 9> pattern{1, 2, 3, 4, 0, 6, 7, 8, 9};
    drawSprite(target, Sprite{10, 4, 9, 1, pattern.data()});
    check(pixelAt(buffer, 10, 4) == 1);
    check(pixelAt(buffer, 13, 4) == 4);
    check(pixelAt(buffer, 14, 4) == 9);
    check(pixelAt(buffer, 18, 4) == 9);
}

void testFixedWidthAndPanelClip() {
    PixelBuffer buffer;
    auto left = makeRenderTarget(buffer, Screen::Left);
    auto right = makeRenderTarget(buffer, Screen::Right);
    clear(left, 2);
    clear(right, 3);

    std::array<ColorIndex, 24> pattern{};
    pattern.fill(7);
    drawSprite(left, Sprite{4, 1, 8, 1, pattern.data()});
    check(pixelAt(buffer, 4, 1) == 7);
    check(pixelAt(buffer, 11, 1) == 7);
    check(pixelAt(buffer, 12, 1) == 2);

    drawSprite(left, Sprite{20, 1, 16, 1, pattern.data()});
    check(pixelAt(buffer, 20, 1) == 7);
    check(pixelAt(buffer, 35, 1) == 7);
    check(pixelAt(buffer, 36, 1) == 2);

    drawSprite(left, Sprite{150, 2, 24, 1, pattern.data()});
    check(pixelAt(buffer, 149, 2) == 2);
    check(pixelAt(buffer, 150, 2) == 7);
    check(pixelAt(buffer, 159, 2) == 7);
    check(pixelAt(buffer, 160, 2) == 3);

    drawSprite(right, Sprite{-4, 3, 24, 1, pattern.data()});
    check(pixelAt(buffer, 159, 3) == 2);
    check(pixelAt(buffer, 160, 3) == 7);
    check(pixelAt(buffer, 179, 3) == 7);
    check(pixelAt(buffer, 180, 3) == 3);
}

void testNearestScale() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    clear(target, 8);

    constexpr std::array<ColorIndex, 4> pattern{1, 2, 3, 0};
    drawSpriteEx(target, SpriteEx{4, 5, 4, 4, 2, 2, pattern.data()});
    check(pixelAt(buffer, 4, 5) == 1);
    check(pixelAt(buffer, 5, 6) == 1);
    check(pixelAt(buffer, 6, 5) == 2);
    check(pixelAt(buffer, 4, 7) == 3);
    check(pixelAt(buffer, 6, 7) == 8);

    constexpr std::array<ColorIndex, 16> downscalePattern{
        1, 1, 2, 2,
        1, 1, 2, 2,
        3, 3, 4, 4,
        3, 3, 4, 4,
    };
    drawSpriteEx(target, SpriteEx{10, 10, 1, 1, 4, 4, downscalePattern.data()});
    check(pixelAt(buffer, 10, 10) == 4);

    constexpr std::array<ColorIndex, 4> clippedPattern{6, 6, 6, 6};
    drawSpriteEx(target,
                 SpriteEx{-2, -2, 4, 4, 2, 2, clippedPattern.data()});
    check(pixelAt(buffer, 0, 0) == 6);
    check(pixelAt(buffer, 2, 0) == 8);
}

void testAllocatorAndBuckets() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    clear(target, 0);

    constexpr std::array<ColorIndex, 1> first{4};
    constexpr std::array<ColorIndex, 1> second{5};
    SpriteBuckets<2, 1> buckets;
    check(buckets.addSprite(20, Sprite{1, 1, 1, 1, second.data()}) == 0);
    check(buckets.addSprite(10, Sprite{1, 1, 1, 1, first.data()}) == 1);
    check(buckets.addSprite(10, Sprite{2, 2, 1, 1, first.data()}) == kInvalidSpriteIndex);
    check(buckets.addSprite(kBucketCount, Sprite{2, 2, 1, 1, first.data()})
           == kInvalidSpriteIndex);
    buckets.draw(target);
    check(pixelAt(buffer, 1, 1) == 5);

    buckets.reset();
    check(buckets.sprites().size() == 0);
    buckets.draw(target);
    check(pixelAt(buffer, 1, 1) == 5);
}

} // namespace

int main() {
    testGenericAndTransparent();
    testFixedWidthAndPanelClip();
    testNearestScale();
    testAllocatorAndBuckets();
    return 0;
}
