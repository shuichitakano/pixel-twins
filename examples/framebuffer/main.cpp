#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/render_target.hpp"
#include "pixel_twins/sdl_presenter.hpp"

#include <chrono>
#include <string_view>
#include <thread>

namespace {

void drawTestPattern(pixel_twins::Framebuffer& framebuffer) {
    using namespace pixel_twins;

    static_cast<void>(framebuffer.setPaletteColor(2, Rgb{32, 80, 160}));
    static_cast<void>(framebuffer.setPaletteColor(3, Rgb{160, 48, 64}));
    static_cast<void>(framebuffer.setPaletteColor(4, Rgb{240, 192, 48}));

    auto left = makeRenderTarget(framebuffer.drawBuffer(), Screen::Left);
    auto right = makeRenderTarget(framebuffer.drawBuffer(), Screen::Right);
    clear(left, 2);
    clear(right, 3);

    for (std::int16_t y = 0; y < static_cast<std::int16_t>(kScreenHeight); ++y) {
        const auto x = static_cast<std::int16_t>(y * 4 / 3);
        static_cast<void>(putPixel(left, x, y, kWhiteColor));
        constexpr auto panelWidth = static_cast<std::int16_t>(kPanelWidth);
        static_cast<void>(putPixel(right, static_cast<std::int16_t>(panelWidth - 1 - x), y, 4));
    }
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
