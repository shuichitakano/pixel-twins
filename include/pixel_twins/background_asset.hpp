#pragma once

#include "pixel_twins/background.hpp"

#include <cstddef>
#include <cstdint>

namespace pixel_twins {

class BackgroundAssetPackView {
public:
    BackgroundAssetPackView() noexcept = default;
    BackgroundAssetPackView(const std::uint8_t* data, std::size_t size) noexcept;

    [[nodiscard]] bool reset(const std::uint8_t* data,
                             std::size_t size) noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool resetSplit(const std::uint8_t* metadata,
                                  std::size_t metadataSize,
                                  const ColorIndex* patterns,
                                  std::size_t patternSize) noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::uint8_t tileWidth() const noexcept;
    [[nodiscard]] std::uint8_t tileHeight() const noexcept;
    [[nodiscard]] std::uint8_t patternCount() const noexcept;
    [[nodiscard]] std::uint8_t terrainCount() const noexcept;
    [[nodiscard]] std::uint8_t variantsPerTerrain() const noexcept;
    [[nodiscard]] std::uint8_t boundaryCount() const noexcept;
    [[nodiscard]] std::uint8_t masksPerBoundary() const noexcept;
    [[nodiscard]] std::uint8_t objectCount() const noexcept;

    [[nodiscard]] bool terrainPattern(std::uint8_t terrain,
                                      std::uint8_t variant,
                                      std::uint8_t& result) const noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool boundaryPattern(std::uint8_t boundary,
                                       std::uint8_t mask,
                                       std::uint8_t& result) const noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool objectPattern(std::uint8_t object,
                                     std::uint8_t& result) const noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] const ColorIndex* patterns() const noexcept;

    [[nodiscard]] Background makeBackground(std::uint16_t width,
                                            std::uint16_t height,
                                            const std::uint8_t* tilemap,
                                            std::uint8_t tileIndexMask = 0xff) const noexcept;

private:
    const std::uint8_t* mapping_ = nullptr;
    const ColorIndex* patterns_ = nullptr;
    std::uint8_t tileWidth_ = 0;
    std::uint8_t tileHeight_ = 0;
    std::uint8_t patternCount_ = 0;
    std::uint8_t terrainCount_ = 0;
    std::uint8_t variantsPerTerrain_ = 0;
    std::uint8_t boundaryCount_ = 0;
    std::uint8_t masksPerBoundary_ = 0;
    std::uint8_t objectCount_ = 0;
};

} // namespace pixel_twins
