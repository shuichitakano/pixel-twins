#pragma once

#include "pixel_twins/platform.hpp"
#include "pixel_twins/render_target.hpp"

#include <cstdint>

namespace pixel_twins {

struct Background {
    std::uint8_t tileWidth;
    std::uint8_t tileHeight;
    std::uint16_t width;
    std::uint16_t height;
    const std::uint8_t* tilemap;
    const std::uint8_t* patterns;
    std::uint8_t tileIndexMask = 0xff;
};

// RenderTargetの左上が仮想画面の(sourceX, sourceY)を参照するようにBGを描画します。
void drawBackground(RenderTarget target,
                    const Background& background,
                    std::int32_t sourceX,
                    std::int32_t sourceY) noexcept PIXEL_TWINS_SRAM;

} // namespace pixel_twins
