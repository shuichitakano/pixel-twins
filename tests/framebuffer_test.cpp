#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/render_target.hpp"

#include <cassert>

int main() {
    using namespace pixel_twins;

    static_assert(kFramebufferSize == 38'400);
    static_assert(sizeof(PixelBuffer) == 38'400);
    static_assert(sizeof(Palette) == 768);

    Framebuffer framebuffer;
    assert(framebuffer.palette()[0].r == 0);
    assert(framebuffer.palette()[1].r == 0);
    assert(framebuffer.palette()[255].r == 255);
    assert(!framebuffer.setPaletteColor(0, Rgb{1, 2, 3}));
    assert(!framebuffer.setPaletteColor(1, Rgb{1, 2, 3}));
    assert(!framebuffer.setPaletteColor(255, Rgb{1, 2, 3}));
    assert(framebuffer.setPaletteColor(2, Rgb{1, 2, 3}));
    assert(framebuffer.palette()[2].r == 1);

    auto left = makeRenderTarget(framebuffer.drawBuffer(), Screen::Left);
    auto right = makeRenderTarget(framebuffer.drawBuffer(), Screen::Right);
    assert(left.width == 160 && left.originX == 0 && left.stride == 320);
    assert(right.width == 160 && right.originX == 160 && right.stride == 320);

    clear(left, 2);
    clear(right, 3);
    assert(framebuffer.drawBuffer()[0] == 2);
    assert(framebuffer.drawBuffer()[159] == 2);
    assert(framebuffer.drawBuffer()[160] == 3);
    assert(framebuffer.drawBuffer()[319] == 3);

    assert(putPixel(right, 0, 0, 4));
    assert(framebuffer.drawBuffer()[160] == 4);
    assert(!putPixel(right, -1, 0, 5));
    assert(!putPixel(right, 160, 0, 5));
    assert(framebuffer.drawBuffer()[159] == 2);

    framebuffer.flip();
    assert(framebuffer.displayBuffer()[160] == 4);
    return 0;
}
