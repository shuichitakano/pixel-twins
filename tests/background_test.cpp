#include "pixel_twins/background.hpp"
#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/render_target.hpp"
#include "test_check.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace pixel_twins;

constexpr std::size_t kTileSize = 8;
constexpr std::size_t kTilePixels = kTileSize * kTileSize;

ColorIndex pixelAt(const PixelBuffer& buffer, std::size_t x, std::size_t y) {
    return buffer[y * kScreenWidth + x];
}

struct TestBackground {
    std::array<ColorIndex, 4> tilemap{0, 1, 2, 3};
    std::array<ColorIndex, 4 * kTilePixels> patterns{};

    TestBackground() {
        for (std::size_t tile = 0; tile < 4; ++tile) {
            for (std::size_t pixel = 0; pixel < kTilePixels; ++pixel) {
                patterns[tile * kTilePixels + pixel] = static_cast<ColorIndex>(tile + 1);
            }
        }
    }

    [[nodiscard]] Background view() const {
        return Background{8, 8, 2, 2, tilemap.data(), patterns.data()};
    }
};

void testScrollAndOutside() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    TestBackground data;

    drawBackground(target, data.view(), 4, 0);
    check(pixelAt(buffer, 0, 0) == 1);
    check(pixelAt(buffer, 3, 0) == 1);
    check(pixelAt(buffer, 4, 0) == 2);
    check(pixelAt(buffer, 11, 0) == 2);
    check(pixelAt(buffer, 12, 0) == 0);
    check(pixelAt(buffer, 0, 8) == 3);
    check(pixelAt(buffer, 4, 8) == 4);
    check(pixelAt(buffer, 0, 16) == 0);

    drawBackground(target, data.view(), -4, -2);
    check(pixelAt(buffer, 0, 0) == 0);
    check(pixelAt(buffer, 4, 2) == 1);
    check(pixelAt(buffer, 11, 2) == 1);
    check(pixelAt(buffer, 12, 2) == 2);
}

void testRightPanelClip() {
    PixelBuffer buffer;
    auto left = makeRenderTarget(buffer, Screen::Left);
    auto right = makeRenderTarget(buffer, Screen::Right);
    clear(left, 9);
    clear(right, 8);
    TestBackground data;

    drawBackground(right, data.view(), 0, 0);
    check(pixelAt(buffer, 159, 0) == 9);
    check(pixelAt(buffer, 160, 0) == 1);
    check(pixelAt(buffer, 167, 0) == 1);
    check(pixelAt(buffer, 168, 0) == 2);
    check(pixelAt(buffer, 176, 0) == 0);
    check(pixelAt(buffer, 319, 119) == 0);
}

void testIndependentPowerOfTwoSize() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    const std::array<ColorIndex, 1> tilemap{0};
    std::array<ColorIndex, 16 * 8> patterns{};
    patterns.fill(6);
    const Background background{16, 8, 1, 1, tilemap.data(), patterns.data()};

    drawBackground(target, background, 15, 7);
    check(pixelAt(buffer, 0, 0) == 6);
    check(pixelAt(buffer, 1, 0) == 0);
    check(pixelAt(buffer, 0, 1) == 0);
}

void testInvalidBackgroundDoesNotDraw() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    clear(target, 7);
    const Background invalid{7, 8, 1, 1, nullptr, nullptr};
    drawBackground(target, invalid, 0, 0);
    check(pixelAt(buffer, 0, 0) == 7);
    check(pixelAt(buffer, 159, 119) == 7);

    const std::array<ColorIndex, 1> tilemap{0};
    const std::array<ColorIndex, 24 * 8> patterns{};
    const Background notPowerOfTwo{24, 8, 1, 1, tilemap.data(), patterns.data()};
    drawBackground(target, notPowerOfTwo, 0, 0);
    check(pixelAt(buffer, 0, 0) == 7);
    check(pixelAt(buffer, 159, 119) == 7);
}

void testExtremeSignedOffsets() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    TestBackground data;

    clear(target, 7);
    drawBackground(target, data.view(), INT32_MIN, INT32_MIN);
    check(pixelAt(buffer, 0, 0) == 0);
    check(pixelAt(buffer, 159, 119) == 0);

    clear(target, 7);
    drawBackground(target, data.view(), INT32_MAX, INT32_MAX);
    check(pixelAt(buffer, 0, 0) == 0);
    check(pixelAt(buffer, 159, 119) == 0);
}

} // namespace

int main() {
    testScrollAndOutside();
    testRightPanelClip();
    testIndependentPowerOfTwoSize();
    testInvalidBackgroundDoesNotDraw();
    testExtremeSignedOffsets();
    return 0;
}
