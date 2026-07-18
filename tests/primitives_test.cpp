#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/primitives.hpp"
#include "pixel_twins/render_target.hpp"
#include "test_check.hpp"

#include <cstddef>

namespace {

using namespace pixel_twins;

ColorIndex pixelAt(const PixelBuffer& buffer, std::size_t x, std::size_t y) {
    return buffer[y * kScreenWidth + x];
}

void testRectangles() {
    PixelBuffer buffer;
    auto left = makeRenderTarget(buffer, Screen::Left);
    auto right = makeRenderTarget(buffer, Screen::Right);
    clear(left, 0);
    clear(right, 9);

    fillRectangle(left, -3, 2, 8, 3, 4);
    check(pixelAt(buffer, 0, 2) == 4);
    check(pixelAt(buffer, 4, 4) == 4);
    check(pixelAt(buffer, 5, 4) == 0);
    check(pixelAt(buffer, 160, 2) == 9);

    drawRectangle(left, 10, 10, 5, 4, 6);
    check(pixelAt(buffer, 10, 10) == 6);
    check(pixelAt(buffer, 14, 13) == 6);
    check(pixelAt(buffer, 12, 11) == 0);
}

void testLines() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    clear(target, 0);

    drawLine(target, -20, 5, 20, 5, 3);
    check(pixelAt(buffer, 0, 5) == 3);
    check(pixelAt(buffer, 20, 5) == 3);
    drawLine(target, 4, -10, 4, 10, 5);
    check(pixelAt(buffer, 4, 0) == 5);
    check(pixelAt(buffer, 4, 10) == 5);
    drawLine(target, -10, -10, -2, -2, 7);
    check(pixelAt(buffer, 0, 0) == 0);
}

void testTriangles() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    clear(target, 0);

    fillTriangle(target, 2, 2, 8, 2, 2, 8, 4);
    check(pixelAt(buffer, 2, 2) == 4);
    check(pixelAt(buffer, 4, 4) == 4);
    check(pixelAt(buffer, 5, 5) == 0);
    check(pixelAt(buffer, 8, 8) == 0);
    fillTriangle(target, 2, 8, 8, 2, 8, 8, 5);
    check(pixelAt(buffer, 7, 7) == 5);
}

void testEllipses() {
    PixelBuffer buffer;
    auto target = makeRenderTarget(buffer, Screen::Left);
    clear(target, 0);

    fillEllipse(target, 10, 10, 4, 2, 3);
    check(pixelAt(buffer, 10, 10) == 3);
    check(pixelAt(buffer, 6, 10) == 3);
    check(pixelAt(buffer, 14, 10) == 3);
    check(pixelAt(buffer, 10, 8) == 3);
    check(pixelAt(buffer, 6, 8) == 0);

    drawEllipse(target, 20, 10, 4, 2, 6);
    check(pixelAt(buffer, 16, 10) == 6);
    check(pixelAt(buffer, 24, 10) == 6);
    check(pixelAt(buffer, 20, 8) == 6);
    check(pixelAt(buffer, 20, 10) == 0);

    fillCircle(target, 0, 0, 3, 7);
    check(pixelAt(buffer, 0, 0) == 7);
    check(pixelAt(buffer, 3, 0) == 7);
    check(pixelAt(buffer, 4, 0) == 0);

    clear(target, 2);
    fillEllipse(target, 10, 10, UINT16_MAX, 2, 7);
    check(pixelAt(buffer, 10, 10) == 2);
}

} // namespace

int main() {
    testRectangles();
    testLines();
    testTriangles();
    testEllipses();
    return 0;
}
