#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/rp2350/board_pins.hpp"
#include "pixel_twins/rp2350/led_panel.hpp"

#include "pico/stdlib.h"

#include <cstddef>
#include <cstdint>

namespace {

// 大きなフレームバッファをスタックに置かない。
pixel_twins::Framebuffer framebuffer;
pixel_twins::rp2350::LedPanelDriver ledPanel;

constexpr std::size_t kColorCount = 252;
constexpr pixel_twins::ColorIndex kFirstColor = 2;

[[nodiscard]] constexpr std::uint8_t colorRamp(std::size_t position) noexcept {
    return static_cast<std::uint8_t>((position * 255u) / 41u);
}

[[nodiscard]] constexpr pixel_twins::Rgb colorWheel(std::size_t hue) noexcept {
    const auto sector = hue / 42u;
    const auto rising = colorRamp(hue % 42u);
    const auto falling = static_cast<std::uint8_t>(255u - rising);

    switch (sector) {
    case 0: return {255, rising, 0};
    case 1: return {falling, 255, 0};
    case 2: return {0, 255, rising};
    case 3: return {0, falling, 255};
    case 4: return {rising, 0, 255};
    default: return {255, 0, falling};
    }
}

void initializeTestPalette() {
    using namespace pixel_twins;

    for (std::size_t hue = 0; hue < kColorCount; ++hue) {
        static_cast<void>(framebuffer.setPaletteColor(
            static_cast<ColorIndex>(kFirstColor + hue), colorWheel(hue)));
    }
}

void makeScrollingTestPattern(std::size_t scrollOffset) {
    auto& pixels = framebuffer.drawBuffer();
    for (std::size_t y = 0; y < pixel_twins::kScreenHeight; ++y) {
        // 4ラインずらして、y=59/60の走査境界をタイル境界に合わせる。
        const auto tileY = (y + 4u) >> 3u;
        for (std::size_t x = 0; x < pixel_twins::kScreenWidth; ++x) {
            const auto tileX = (x + scrollOffset) >> 3u;
            const auto hue = (tileX * 11u + tileY * 29u) % kColorCount;
            pixels[y * pixel_twins::kScreenWidth + x] =
                static_cast<pixel_twins::ColorIndex>(kFirstColor + hue);
        }
    }
}

} // namespace

int main() {
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    initializeTestPalette();
    ledPanel.initialize();
    ledPanel.setPalette(framebuffer.palette());

    constexpr std::size_t kHeartbeatFrames = 30;
    std::size_t heartbeatFrame = 0;
    std::size_t scrollOffset = 0;
    bool heartbeatLed = true;
    while (true) {
        makeScrollingTestPattern(scrollOffset);
        framebuffer.flip();
        ledPanel.present(framebuffer.displayBuffer());
        if (++scrollOffset == kColorCount * 8u) scrollOffset = 0;

        if (++heartbeatFrame == kHeartbeatFrames) {
            heartbeatFrame = 0;
            heartbeatLed = !heartbeatLed;
            gpio_put(PICO_DEFAULT_LED_PIN, heartbeatLed);
        }
    }
}
