#include "pixel_twins/background_asset.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace pixel_twins {
namespace {

constexpr std::size_t kHeaderSize = 32;

[[nodiscard]] std::uint16_t read16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8U);
}

[[nodiscard]] std::uint32_t read32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0])
        | (static_cast<std::uint32_t>(p[1]) << 8U)
        | (static_cast<std::uint32_t>(p[2]) << 16U)
        | (static_cast<std::uint32_t>(p[3]) << 24U);
}

[[nodiscard]] bool validTileSize(std::uint8_t value) noexcept {
    return value >= 8 && value <= 128
        && (value & static_cast<std::uint8_t>(value - 1U)) == 0;
}

} // namespace

BackgroundAssetPackView::BackgroundAssetPackView(const std::uint8_t* data,
                                                 std::size_t size) noexcept {
    static_cast<void>(reset(data, size));
}

bool BackgroundAssetPackView::reset(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < kHeaderSize || data[0] != 'P' || data[1] != 'T'
        || data[2] != 'B' || data[3] != 'G' || read16(data + 4) != 1
        || read16(data + 6) != kHeaderSize) {
        *this = BackgroundAssetPackView{};
        return false;
    }
    const auto patternDataOffset = read32(data + 20);
    const auto patternDataSize = read32(data + 24);
    if (patternDataOffset > size || patternDataSize > size - patternDataOffset) {
        *this = BackgroundAssetPackView{};
        return false;
    }
    return resetSplit(data,
                      patternDataOffset,
                      data + patternDataOffset,
                      patternDataSize);
}

bool BackgroundAssetPackView::resetSplit(const std::uint8_t* data,
                                         std::size_t metadataSize,
                                         const ColorIndex* patternData,
                                         std::size_t suppliedPatternSize) noexcept {
    *this = BackgroundAssetPackView{};
    if (data == nullptr || patternData == nullptr || metadataSize < kHeaderSize
        || data[0] != 'P' || data[1] != 'T' || data[2] != 'B' || data[3] != 'G'
        || read16(data + 4) != 1 || read16(data + 6) != kHeaderSize) {
        return false;
    }
    const auto tileWidth = data[8];
    const auto tileHeight = data[9];
    const auto patternCount = data[10];
    const auto terrainCount = data[11];
    const auto variants = data[12];
    const auto boundaryCount = data[13];
    const auto masks = data[14];
    const auto objectCount = data[15];
    const auto mappingOffset = read32(data + 16);
    const auto patternDataOffset = read32(data + 20);
    const auto patternDataSize = read32(data + 24);
    const auto mappingSize = static_cast<std::uint32_t>(terrainCount) * variants
        + static_cast<std::uint32_t>(boundaryCount) * masks + objectCount;
    const auto tilePixels = static_cast<std::uint32_t>(tileWidth) * tileHeight;
    if (!validTileSize(tileWidth) || !validTileSize(tileHeight)
        || patternCount == 0 || patternCount > 128
        || terrainCount == 0 || variants == 0 || boundaryCount == 0 || masks == 0
        || mappingOffset < kHeaderSize || mappingOffset > metadataSize
        || mappingSize > metadataSize - mappingOffset
        || patternDataOffset < mappingOffset + mappingSize || patternDataOffset > metadataSize
        || patternDataOffset - (mappingOffset + mappingSize) > 3
        || (patternDataOffset & 3U) != 0
        || patternDataSize != static_cast<std::uint32_t>(patternCount) * tilePixels
        || patternDataSize > suppliedPatternSize) {
        return false;
    }
    for (std::uint32_t index = 0; index < mappingSize; ++index) {
        if (data[mappingOffset + index] >= patternCount) {
            return false;
        }
    }
    mapping_ = data + mappingOffset;
    patterns_ = patternData;
    tileWidth_ = tileWidth;
    tileHeight_ = tileHeight;
    patternCount_ = patternCount;
    terrainCount_ = terrainCount;
    variantsPerTerrain_ = variants;
    boundaryCount_ = boundaryCount;
    masksPerBoundary_ = masks;
    objectCount_ = objectCount;
    return true;
}

bool BackgroundAssetPackView::valid() const noexcept { return mapping_ != nullptr; }
std::uint8_t BackgroundAssetPackView::tileWidth() const noexcept { return tileWidth_; }
std::uint8_t BackgroundAssetPackView::tileHeight() const noexcept { return tileHeight_; }
std::uint8_t BackgroundAssetPackView::patternCount() const noexcept { return patternCount_; }
std::uint8_t BackgroundAssetPackView::terrainCount() const noexcept { return terrainCount_; }
std::uint8_t BackgroundAssetPackView::variantsPerTerrain() const noexcept { return variantsPerTerrain_; }
std::uint8_t BackgroundAssetPackView::boundaryCount() const noexcept { return boundaryCount_; }
std::uint8_t BackgroundAssetPackView::masksPerBoundary() const noexcept { return masksPerBoundary_; }
std::uint8_t BackgroundAssetPackView::objectCount() const noexcept { return objectCount_; }

bool BackgroundAssetPackView::terrainPattern(std::uint8_t terrain,
                                             std::uint8_t variant,
                                             std::uint8_t& result) const noexcept {
    if (!valid() || terrain >= terrainCount_ || variant >= variantsPerTerrain_) {
        return false;
    }
    result = mapping_[static_cast<std::size_t>(terrain) * variantsPerTerrain_ + variant];
    return true;
}

bool BackgroundAssetPackView::boundaryPattern(std::uint8_t boundary,
                                              std::uint8_t mask,
                                              std::uint8_t& result) const noexcept {
    if (!valid() || boundary >= boundaryCount_ || mask >= masksPerBoundary_) {
        return false;
    }
    const auto offset = static_cast<std::size_t>(terrainCount_) * variantsPerTerrain_
        + static_cast<std::size_t>(boundary) * masksPerBoundary_ + mask;
    result = mapping_[offset];
    return true;
}

bool BackgroundAssetPackView::objectPattern(std::uint8_t object,
                                            std::uint8_t& result) const noexcept {
    if (!valid() || object >= objectCount_) {
        return false;
    }
    const auto offset = static_cast<std::size_t>(terrainCount_) * variantsPerTerrain_
        + static_cast<std::size_t>(boundaryCount_) * masksPerBoundary_ + object;
    result = mapping_[offset];
    return true;
}

const ColorIndex* BackgroundAssetPackView::patterns() const noexcept { return patterns_; }

Background BackgroundAssetPackView::makeBackground(std::uint16_t width,
                                                   std::uint16_t height,
                                                   const std::uint8_t* tilemap,
                                                   std::uint8_t tileIndexMask) const noexcept {
    if (!valid()) {
        return Background{};
    }
    return Background{tileWidth_, tileHeight_, width, height, tilemap, patterns_, tileIndexMask};
}

} // namespace pixel_twins
