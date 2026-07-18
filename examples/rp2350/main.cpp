#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/primitives.hpp"
#include "pixel_twins/render_target.hpp"

#include "pico/stdlib.h"

namespace {

// 大きなフレームバッファをスタックに置かない。
pixel_twins::Framebuffer framebuffer;

} // namespace

int main() {
    stdio_init_all();
    const auto target = pixel_twins::makeRenderTarget(
        framebuffer.drawBuffer(), pixel_twins::Screen::Full);
    pixel_twins::fillRectangle(target, 0, 0, 320, 120, pixel_twins::kDrawableBlackColor);
    pixel_twins::drawRectangle(target, 0, 0, 320, 120, pixel_twins::kWhiteColor);
    framebuffer.flip();

    // LED転送はパネルの電気仕様とPIO/DMA方式の確定後に接続する。
    while (true) tight_loop_contents();
}
