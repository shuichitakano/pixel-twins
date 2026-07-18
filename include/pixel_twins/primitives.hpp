#pragma once

#include "pixel_twins/platform.hpp"
#include "pixel_twins/render_target.hpp"

#include <cstdint>

namespace pixel_twins {

void fillRectangle(RenderTarget target,
                   std::int16_t x,
                   std::int16_t y,
                   std::uint16_t width,
                   std::uint16_t height,
                   ColorIndex color) noexcept PIXEL_TWINS_SRAM;

void drawRectangle(RenderTarget target,
                   std::int16_t x,
                   std::int16_t y,
                   std::uint16_t width,
                   std::uint16_t height,
                   ColorIndex color) noexcept PIXEL_TWINS_SRAM;

void drawLine(RenderTarget target,
              std::int16_t x0,
              std::int16_t y0,
              std::int16_t x1,
              std::int16_t y1,
              ColorIndex color) noexcept PIXEL_TWINS_SRAM;

void fillTriangle(RenderTarget target,
                  std::int16_t x0,
                  std::int16_t y0,
                  std::int16_t x1,
                  std::int16_t y1,
                  std::int16_t x2,
                  std::int16_t y2,
                  ColorIndex color) noexcept PIXEL_TWINS_SRAM;

void fillEllipse(RenderTarget target,
                 std::int16_t centerX,
                 std::int16_t centerY,
                 std::uint16_t radiusX,
                 std::uint16_t radiusY,
                 ColorIndex color) noexcept PIXEL_TWINS_SRAM;

void drawEllipse(RenderTarget target,
                 std::int16_t centerX,
                 std::int16_t centerY,
                 std::uint16_t radiusX,
                 std::uint16_t radiusY,
                 ColorIndex color) noexcept PIXEL_TWINS_SRAM;

inline void fillCircle(RenderTarget target,
                       std::int16_t centerX,
                       std::int16_t centerY,
                       std::uint16_t radius,
                       ColorIndex color) noexcept PIXEL_TWINS_SRAM {
    fillEllipse(target, centerX, centerY, radius, radius, color);
}

inline void drawCircle(RenderTarget target,
                       std::int16_t centerX,
                       std::int16_t centerY,
                       std::uint16_t radius,
                       ColorIndex color) noexcept PIXEL_TWINS_SRAM {
    drawEllipse(target, centerX, centerY, radius, radius, color);
}

} // namespace pixel_twins
