#include "pixel_twins/primitives.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace pixel_twins {
namespace {

PIXEL_TWINS_FORCE_INLINE void fillEight(ColorIndex* destination,
                                        ColorIndex color) noexcept {
    destination[0] = color;
    destination[1] = color;
    destination[2] = color;
    destination[3] = color;
    destination[4] = color;
    destination[5] = color;
    destination[6] = color;
    destination[7] = color;
}

void fillHorizontal(RenderTarget target,
                    std::int32_t x0,
                    std::int32_t x1,
                    std::int32_t y,
                    ColorIndex color) noexcept PIXEL_TWINS_SRAM;
void fillHorizontal(RenderTarget target,
                    std::int32_t x0,
                    std::int32_t x1,
                    std::int32_t y,
                    ColorIndex color) noexcept {
    if (y < 0 || y >= target.height || x1 < 0 || x0 >= target.width || x0 > x1) {
        return;
    }
    x0 = std::max<std::int32_t>(0, x0);
    x1 = std::min<std::int32_t>(target.width - 1, x1);
    auto* destination = target.pixels
        + static_cast<std::size_t>(target.originY + y) * target.stride
        + static_cast<std::size_t>(target.originX + x0);
    auto count = static_cast<std::uint32_t>(x1 - x0 + 1);
    while (count >= 8) {
        fillEight(destination, color);
        destination += 8;
        count -= 8;
    }
    while (count > 0) {
        *destination++ = color;
        --count;
    }
}

enum : std::uint8_t {
    kClipLeft = 1,
    kClipRight = 2,
    kClipTop = 4,
    kClipBottom = 8,
};

[[nodiscard]] PIXEL_TWINS_FORCE_INLINE std::uint8_t clipCode(RenderTarget target,
                                                              std::int32_t x,
                                                              std::int32_t y) noexcept {
    std::uint8_t code = 0;
    if (x < 0) code |= kClipLeft;
    else if (x >= target.width) code |= kClipRight;
    if (y < 0) code |= kClipTop;
    else if (y >= target.height) code |= kClipBottom;
    return code;
}

[[nodiscard]] bool clipLine(RenderTarget target,
                            std::int32_t& x0,
                            std::int32_t& y0,
                            std::int32_t& x1,
                            std::int32_t& y1) noexcept PIXEL_TWINS_SRAM;
bool clipLine(RenderTarget target,
              std::int32_t& x0,
              std::int32_t& y0,
              std::int32_t& x1,
              std::int32_t& y1) noexcept {
    auto code0 = clipCode(target, x0, y0);
    auto code1 = clipCode(target, x1, y1);
    while (true) {
        if ((code0 | code1) == 0) {
            return true;
        }
        if ((code0 & code1) != 0) {
            return false;
        }

        const auto code = code0 != 0 ? code0 : code1;
        std::int32_t x = 0;
        std::int32_t y = 0;
        if ((code & kClipTop) != 0) {
            y = 0;
            x = x0 + static_cast<std::int32_t>(
                static_cast<std::int64_t>(x1 - x0) * (y - y0) / (y1 - y0));
        } else if ((code & kClipBottom) != 0) {
            y = target.height - 1;
            x = x0 + static_cast<std::int32_t>(
                static_cast<std::int64_t>(x1 - x0) * (y - y0) / (y1 - y0));
        } else if ((code & kClipRight) != 0) {
            x = target.width - 1;
            y = y0 + static_cast<std::int32_t>(
                static_cast<std::int64_t>(y1 - y0) * (x - x0) / (x1 - x0));
        } else {
            x = 0;
            y = y0 + static_cast<std::int32_t>(
                static_cast<std::int64_t>(y1 - y0) * (x - x0) / (x1 - x0));
        }

        if (code == code0) {
            x0 = x;
            y0 = y;
            code0 = clipCode(target, x0, y0);
        } else {
            x1 = x;
            y1 = y;
            code1 = clipCode(target, x1, y1);
        }
    }
}

[[nodiscard]] PIXEL_TWINS_FORCE_INLINE std::int64_t edge(std::int32_t ax,
                                                          std::int32_t ay,
                                                          std::int32_t bx,
                                                          std::int32_t by,
                                                          std::int32_t px,
                                                          std::int32_t py) noexcept {
    return static_cast<std::int64_t>(px - ax) * (by - ay)
        - static_cast<std::int64_t>(py - ay) * (bx - ax);
}

PIXEL_TWINS_FORCE_INLINE void plotPoint(RenderTarget target,
                                        std::int32_t x,
                                        std::int32_t y,
                                        ColorIndex color) noexcept {
    if (x < 0 || y < 0 || x >= target.width || y >= target.height) {
        return;
    }
    const auto offset = static_cast<std::size_t>(target.originY + y) * target.stride
        + static_cast<std::size_t>(target.originX + x);
    target.pixels[offset] = color;
}

PIXEL_TWINS_FORCE_INLINE void plotPointUnchecked(RenderTarget target,
                                                 std::int32_t x,
                                                 std::int32_t y,
                                                 ColorIndex color) noexcept {
    const auto offset = static_cast<std::size_t>(target.originY + y) * target.stride
        + static_cast<std::size_t>(target.originX + x);
    target.pixels[offset] = color;
}

PIXEL_TWINS_FORCE_INLINE void plotEllipsePoints(RenderTarget target,
                                                std::int32_t centerX,
                                                std::int32_t centerY,
                                                std::int64_t x,
                                                std::int64_t y,
                                                ColorIndex color) noexcept {
    plotPoint(target,
              static_cast<std::int32_t>(centerX + x),
              static_cast<std::int32_t>(centerY + y),
              color);
    plotPoint(target,
              static_cast<std::int32_t>(centerX - x),
              static_cast<std::int32_t>(centerY + y),
              color);
    plotPoint(target,
              static_cast<std::int32_t>(centerX + x),
              static_cast<std::int32_t>(centerY - y),
              color);
    plotPoint(target,
              static_cast<std::int32_t>(centerX - x),
              static_cast<std::int32_t>(centerY - y),
              color);
}

} // namespace

void fillRectangle(RenderTarget target,
                   std::int16_t x,
                   std::int16_t y,
                   std::uint16_t width,
                   std::uint16_t height,
                   ColorIndex color) noexcept {
    if (width == 0 || height == 0) {
        return;
    }
    const auto x1 = static_cast<std::int32_t>(x) + width - 1;
    const auto yStart = std::max<std::int32_t>(0, y);
    const auto yEnd = std::min<std::int32_t>(target.height,
                                             static_cast<std::int32_t>(y) + height);
    for (auto destinationY = yStart; destinationY < yEnd; ++destinationY) {
        fillHorizontal(target, x, x1, destinationY, color);
    }
}

void drawRectangle(RenderTarget target,
                   std::int16_t x,
                   std::int16_t y,
                   std::uint16_t width,
                   std::uint16_t height,
                   ColorIndex color) noexcept {
    if (width == 0 || height == 0) {
        return;
    }
    const auto x1 = static_cast<std::int32_t>(x) + width - 1;
    const auto y1 = static_cast<std::int32_t>(y) + height - 1;
    fillHorizontal(target, x, x1, y, color);
    if (height > 1) {
        fillHorizontal(target, x, x1, y1, color);
    }
    if (height <= 2 || width == 1) {
        return;
    }
    const auto yStart = std::max<std::int32_t>(0, static_cast<std::int32_t>(y) + 1);
    const auto yEnd = std::min<std::int32_t>(target.height, y1);
    for (auto destinationY = yStart; destinationY < yEnd; ++destinationY) {
        plotPoint(target, x, destinationY, color);
        if (width > 1) {
            plotPoint(target, x1, destinationY, color);
        }
    }
}

void drawLine(RenderTarget target,
              std::int16_t x0Value,
              std::int16_t y0Value,
              std::int16_t x1Value,
              std::int16_t y1Value,
              ColorIndex color) noexcept {
    auto x0 = static_cast<std::int32_t>(x0Value);
    auto y0 = static_cast<std::int32_t>(y0Value);
    auto x1 = static_cast<std::int32_t>(x1Value);
    auto y1 = static_cast<std::int32_t>(y1Value);
    if (!clipLine(target, x0, y0, x1, y1)) {
        return;
    }

    const auto dx = std::abs(x1 - x0);
    const auto stepX = x0 < x1 ? 1 : -1;
    const auto dy = -std::abs(y1 - y0);
    const auto stepY = y0 < y1 ? 1 : -1;
    auto error = dx + dy;
    while (true) {
        plotPointUnchecked(target, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const auto twiceError = error * 2;
        if (twiceError >= dy) {
            error += dy;
            x0 += stepX;
        }
        if (twiceError <= dx) {
            error += dx;
            y0 += stepY;
        }
    }
}

void fillTriangle(RenderTarget target,
                  std::int16_t x0,
                  std::int16_t y0,
                  std::int16_t x1,
                  std::int16_t y1,
                  std::int16_t x2,
                  std::int16_t y2,
                  ColorIndex color) noexcept {
    const auto minX = std::max<std::int32_t>(0, std::min({x0, x1, x2}));
    const auto maxX = std::min<std::int32_t>(target.width - 1, std::max({x0, x1, x2}));
    const auto minY = std::max<std::int32_t>(0, std::min({y0, y1, y2}));
    const auto maxY = std::min<std::int32_t>(target.height - 1, std::max({y0, y1, y2}));
    if (minX > maxX || minY > maxY) {
        return;
    }

    const auto area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) {
        drawLine(target, x0, y0, x1, y1, color);
        drawLine(target, x1, y1, x2, y2, color);
        return;
    }
    const auto sign = area > 0 ? std::int64_t{1} : std::int64_t{-1};
    // Keep edge values doubled so the half-pixel center offset stays integral.
    const auto stepX0 = static_cast<std::int64_t>(2) * (y2 - y1) * sign;
    const auto stepX1 = static_cast<std::int64_t>(2) * (y0 - y2) * sign;
    const auto stepX2 = static_cast<std::int64_t>(2) * (y1 - y0) * sign;
    const auto stepY0 = static_cast<std::int64_t>(2) * (x1 - x2) * sign;
    const auto stepY1 = static_cast<std::int64_t>(2) * (x2 - x0) * sign;
    const auto stepY2 = static_cast<std::int64_t>(2) * (x0 - x1) * sign;
    auto row0 = (2 * edge(x1, y1, x2, y2, minX, minY)
                 + (y2 - y1) + (x1 - x2)) * sign;
    auto row1 = (2 * edge(x2, y2, x0, y0, minX, minY)
                 + (y0 - y2) + (x2 - x0)) * sign;
    auto row2 = (2 * edge(x0, y0, x1, y1, minX, minY)
                 + (y1 - y0) + (x0 - x1)) * sign;

    for (auto y = minY; y <= maxY; ++y) {
        auto value0 = row0;
        auto value1 = row1;
        auto value2 = row2;
        auto* destination = target.pixels
            + static_cast<std::size_t>(target.originY + y) * target.stride
            + static_cast<std::size_t>(target.originX + minX);
        for (auto x = minX; x <= maxX; ++x) {
            if (value0 >= 0 && value1 >= 0 && value2 >= 0) {
                *destination = color;
            }
            ++destination;
            value0 += stepX0;
            value1 += stepX1;
            value2 += stepX2;
        }
        row0 += stepY0;
        row1 += stepY1;
        row2 += stepY2;
    }
}

void fillEllipse(RenderTarget target,
                 std::int16_t centerX,
                 std::int16_t centerY,
                 std::uint16_t radiusX,
                 std::uint16_t radiusY,
                 ColorIndex color) noexcept {
    if (radiusX > INT16_MAX || radiusY > INT16_MAX) {
        return;
    }
    if (radiusX == 0) {
        const auto startY = std::max<std::int32_t>(0,
                                                   static_cast<std::int32_t>(centerY) - radiusY);
        const auto endY = std::min<std::int32_t>(target.height - 1,
                                                 static_cast<std::int32_t>(centerY) + radiusY);
        for (auto y = startY; y <= endY; ++y) {
            plotPoint(target, centerX, y, color);
        }
        return;
    }
    if (radiusY == 0) {
        fillHorizontal(target,
                       static_cast<std::int32_t>(centerX) - radiusX,
                       static_cast<std::int32_t>(centerX) + radiusX,
                       centerY,
                       color);
        return;
    }

    const auto radiusX2 = static_cast<std::int64_t>(radiusX) * radiusX;
    const auto radiusY2 = static_cast<std::int64_t>(radiusY) * radiusY;
    const auto limit = radiusX2 * radiusY2;
    auto x = static_cast<std::int64_t>(radiusX);
    for (std::int64_t y = 0; y <= radiusY; ++y) {
        while (x > 0 && x * x * radiusY2 + y * y * radiusX2 > limit) {
            --x;
        }
        fillHorizontal(target,
                       static_cast<std::int32_t>(centerX - x),
                       static_cast<std::int32_t>(centerX + x),
                       static_cast<std::int32_t>(centerY + y),
                       color);
        if (y != 0) {
            fillHorizontal(target,
                           static_cast<std::int32_t>(centerX - x),
                           static_cast<std::int32_t>(centerX + x),
                           static_cast<std::int32_t>(centerY - y),
                           color);
        }
    }
}

void drawEllipse(RenderTarget target,
                 std::int16_t centerX,
                 std::int16_t centerY,
                 std::uint16_t radiusX,
                 std::uint16_t radiusY,
                 ColorIndex color) noexcept {
    if (radiusX > INT16_MAX || radiusY > INT16_MAX) {
        return;
    }
    if (radiusX == 0 || radiusY == 0) {
        fillEllipse(target, centerX, centerY, radiusX, radiusY, color);
        return;
    }

    const auto radiusX2 = static_cast<std::int64_t>(radiusX) * radiusX;
    const auto radiusY2 = static_cast<std::int64_t>(radiusY) * radiusY;
    auto x = std::int64_t{0};
    auto y = static_cast<std::int64_t>(radiusY);
    auto dx = std::int64_t{0};
    auto dy = 2 * radiusX2 * y;
    auto decision = radiusY2 - radiusX2 * y + radiusX2 / 4;

    while (dx < dy) {
        plotEllipsePoints(target, centerX, centerY, x, y, color);
        ++x;
        dx += 2 * radiusY2;
        if (decision < 0) {
            decision += radiusY2 + dx;
        } else {
            --y;
            dy -= 2 * radiusX2;
            decision += radiusY2 + dx - dy;
        }
    }

    decision = radiusY2 * (x * x + x) + radiusY2 / 4
        + radiusX2 * (y - 1) * (y - 1) - radiusX2 * radiusY2;
    while (y >= 0) {
        plotEllipsePoints(target, centerX, centerY, x, y, color);
        --y;
        dy -= 2 * radiusX2;
        if (decision > 0) {
            decision += radiusX2 - dy;
        } else {
            ++x;
            dx += 2 * radiusY2;
            decision += radiusX2 - dy + dx;
        }
    }
}

} // namespace pixel_twins
