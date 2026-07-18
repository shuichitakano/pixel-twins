#include "pixel_twins/background.hpp"
#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/primitives.hpp"
#include "pixel_twins/render_target.hpp"
#include "pixel_twins/sdl_presenter.hpp"
#include "pixel_twins/sprite.hpp"

#include <array>
#include <chrono>
#include <string_view>
#include <thread>

namespace {

constexpr std::array<pixel_twins::ColorIndex, 16 * 16> makeSpritePattern() {
    std::array<pixel_twins::ColorIndex, 16 * 16> pattern{};
    for (std::size_t y = 0; y < 16; ++y) {
        for (std::size_t x = 0; x < 16; ++x) {
            const auto border = x == 0 || y == 0 || x == 15 || y == 15;
            const auto diagonal = x == y || x + y == 15;
            pattern[y * 16 + x] = border ? 1 : (diagonal ? 255 : 0);
        }
    }
    return pattern;
}

constexpr auto kSpritePattern = makeSpritePattern();

constexpr std::array<pixel_twins::ColorIndex, 4 * 8 * 8> makeTilePatterns() {
    std::array<pixel_twins::ColorIndex, 4 * 8 * 8> patterns{};
    for (std::size_t tile = 0; tile < 4; ++tile) {
        for (std::size_t y = 0; y < 8; ++y) {
            for (std::size_t x = 0; x < 8; ++x) {
                const auto accent = x == 0 || y == 0;
                patterns[tile * 64 + y * 8 + x] = accent
                    ? static_cast<pixel_twins::ColorIndex>(4 + tile % 2)
                    : static_cast<pixel_twins::ColorIndex>(2 + tile % 2);
            }
        }
    }
    return patterns;
}

constexpr std::size_t kMapWidth = 24;
constexpr std::size_t kMapHeight = 16;

constexpr std::array<std::uint8_t, kMapWidth * kMapHeight> makeTilemap() {
    std::array<std::uint8_t, kMapWidth * kMapHeight> tilemap{};
    for (std::size_t y = 0; y < kMapHeight; ++y) {
        for (std::size_t x = 0; x < kMapWidth; ++x) {
            tilemap[y * kMapWidth + x] = static_cast<std::uint8_t>((x + y) % 4);
        }
    }
    return tilemap;
}

constexpr auto kTilePatterns = makeTilePatterns();
constexpr auto kTilemap = makeTilemap();

void drawTestPattern(pixel_twins::Framebuffer& framebuffer) {
    using namespace pixel_twins;

    static_cast<void>(framebuffer.setPaletteColor(2, Rgb{32, 80, 160}));
    static_cast<void>(framebuffer.setPaletteColor(3, Rgb{160, 48, 64}));
    static_cast<void>(framebuffer.setPaletteColor(4, Rgb{240, 192, 48}));
    static_cast<void>(framebuffer.setPaletteColor(5, Rgb{96, 208, 144}));

    auto left = makeRenderTarget(framebuffer.drawBuffer(), Screen::Left);
    auto right = makeRenderTarget(framebuffer.drawBuffer(), Screen::Right);
    const Background background{8,
                                8,
                                static_cast<std::uint16_t>(kMapWidth),
                                static_cast<std::uint16_t>(kMapHeight),
                                kTilemap.data(),
                                kTilePatterns.data()};
    drawBackground(left, background, 0, 0);
    drawBackground(right, background, 24, 4);

    for (std::int16_t y = 0; y < static_cast<std::int16_t>(kScreenHeight); ++y) {
        const auto x = static_cast<std::int16_t>(y * 4 / 3);
        static_cast<void>(putPixel(left, x, y, kWhiteColor));
        constexpr auto panelWidth = static_cast<std::int16_t>(kPanelWidth);
        static_cast<void>(putPixel(right, static_cast<std::int16_t>(panelWidth - 1 - x), y, 4));
    }

    drawSprite(left, Sprite{72, 52, 16, 16, kSpritePattern.data()});
    drawSpriteEx(right,
                 SpriteEx{64, 44, 32, 32, 16, 16, kSpritePattern.data()});
    drawRectangle(left, 4, 4, 36, 20, kWhiteColor);
    drawLine(left, 4, 24, 40, 4, 5);
    fillTriangle(right, 8, 8, 40, 8, 24, 28, 4);
    drawEllipse(right, 128, 92, 20, 8, kWhiteColor);
    fillCircle(left, 128, 92, 10, 5);
}

} // namespace

int main(int argc, char** argv) {
    const bool once = argc > 1 && std::string_view(argv[1]) == "--once";
    pixel_twins::Framebuffer framebuffer;
    drawTestPattern(framebuffer);
    framebuffer.flip();

    pixel_twins::sdl::Presenter presenter;
    do {
        if (!presenter.processEvents()) {
            break;
        }
        presenter.present(framebuffer);
        if (once) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    } while (true);
    return 0;
}
