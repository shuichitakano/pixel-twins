#pragma once

#include "pixel_twins/platform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace pixel_twins {

using ColorIndex = std::uint8_t;

inline constexpr std::size_t kPanelWidth = 160;
inline constexpr std::size_t kScreenWidth = kPanelWidth * 2;
inline constexpr std::size_t kScreenHeight = 120;
inline constexpr std::size_t kFramebufferSize = kScreenWidth * kScreenHeight;
inline constexpr std::size_t kPaletteSize = 256;

inline constexpr ColorIndex kTransparentColor = 0;
inline constexpr ColorIndex kDrawableBlackColor = 1;
inline constexpr ColorIndex kWhiteColor = 255;

struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

static_assert(sizeof(Rgb) == 3, "RgbはRGB888の3バイトでなければなりません");

using Palette = std::array<Rgb, kPaletteSize>;
using PixelBuffer = std::array<ColorIndex, kFramebufferSize>;

class Framebuffer {
public:
    Framebuffer() noexcept;

    [[nodiscard]] PixelBuffer& drawBuffer() noexcept;
    [[nodiscard]] const PixelBuffer& drawBuffer() const noexcept;
    [[nodiscard]] const PixelBuffer& displayBuffer() const noexcept;

    [[nodiscard]] const Palette& palette() const noexcept;
    [[nodiscard]] bool setPaletteColor(ColorIndex index, Rgb color) noexcept;

    void flip() noexcept PIXEL_TWINS_SRAM;

private:
    std::array<PixelBuffer, 2> buffers_;
    Palette palette_;
    std::uint8_t drawIndex_ = 1;
    std::uint8_t displayIndex_ = 0;
};

} // namespace pixel_twins
