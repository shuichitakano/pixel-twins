#pragma once

#include "pixel_twins/sprite.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace pixel_twins {

struct SpriteAssetInfo {
    std::uint16_t frameCount;
    std::uint8_t columns;
    std::uint8_t rows;
    std::uint8_t logicalWidth;
    std::uint8_t logicalHeight;
};

struct SpriteFramePattern {
    const ColorIndex* pixels;
    std::uint8_t trimX;
    std::uint8_t trimY;
    std::uint8_t width;
    std::uint8_t height;
};

struct SpritePixelRegion {
    std::uint32_t sourceOffset;
    const ColorIndex* pixels;
    std::size_t size;
};

class SpriteAssetPackView {
public:
    static constexpr std::size_t kMaximumPixelRegions = 5;

    SpriteAssetPackView() noexcept = default;
    SpriteAssetPackView(const std::uint8_t* data, std::size_t size) noexcept;

    [[nodiscard]] bool reset(const std::uint8_t* data,
                             std::size_t size) noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool resetSplit(const std::uint8_t* metadata,
                                  std::size_t metadataSize,
                                  const ColorIndex* pixels,
                                  std::size_t pixelSize) noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool resetSplitRegions(
        const std::uint8_t* metadata,
        std::size_t metadataSize,
        const SpritePixelRegion* regions,
        std::size_t regionCount) noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::uint16_t assetCount() const noexcept;

    [[nodiscard]] bool assetInfo(std::uint16_t assetIndex,
                                 SpriteAssetInfo& result) const noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool frame(std::uint16_t assetIndex,
                             std::uint16_t frameIndex,
                             SpriteFramePattern& result) const noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool frameAt(std::uint16_t assetIndex,
                               std::uint8_t column,
                               std::uint8_t row,
                               SpriteFramePattern& result) const noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool makeSprite(std::uint16_t assetIndex,
                                  std::uint16_t frameIndex,
                                  std::int16_t logicalX,
                                  std::int16_t logicalY,
                                  Sprite& result) const noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool makeSpriteAt(std::uint16_t assetIndex,
                                    std::uint8_t column,
                                    std::uint8_t row,
                                    std::int16_t logicalX,
                                    std::int16_t logicalY,
                                    Sprite& result) const noexcept PIXEL_TWINS_SRAM;

private:
    [[nodiscard]] const ColorIndex* pixelsAt(
        std::uint32_t offset) const noexcept PIXEL_TWINS_SRAM;

    const std::uint8_t* data_ = nullptr;
    const std::uint8_t* frameTable_ = nullptr;
    std::array<SpritePixelRegion, kMaximumPixelRegions> pixelRegions_{};
    std::uint8_t pixelRegionCount_ = 0;
    std::uint16_t assetCount_ = 0;
};

} // namespace pixel_twins
