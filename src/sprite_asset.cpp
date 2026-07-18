#include "pixel_twins/sprite_asset.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace pixel_twins {
namespace {

constexpr std::size_t kHeaderSize = 24;
constexpr std::size_t kAssetSize = 12;
constexpr std::size_t kFrameSize = 8;
constexpr std::uint32_t kEmptyPixelOffset = 0xffffffffU;

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

[[nodiscard]] bool multiplyFits(std::size_t left,
                                std::size_t right,
                                std::size_t limit) noexcept {
    return left == 0 || right <= limit / left;
}

} // namespace

SpriteAssetPackView::SpriteAssetPackView(const std::uint8_t* data, std::size_t size) noexcept {
    static_cast<void>(reset(data, size));
}

bool SpriteAssetPackView::reset(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < kHeaderSize || data[0] != 'P' || data[1] != 'T'
        || data[2] != 'S' || data[3] != 'P' || read16(data + 4) != 1
        || read16(data + 6) != kHeaderSize || read16(data + 10) != kAssetSize) {
        *this = SpriteAssetPackView{};
        return false;
    }
    const auto pixelDataOffset = read32(data + 16);
    const auto pixelDataSize = read32(data + 20);
    if (pixelDataOffset > size || pixelDataSize > size - pixelDataOffset) {
        *this = SpriteAssetPackView{};
        return false;
    }
    return resetSplit(data,
                      pixelDataOffset,
                      data + pixelDataOffset,
                      pixelDataSize);
}

bool SpriteAssetPackView::resetSplit(const std::uint8_t* data,
                                     std::size_t metadataSize,
                                     const ColorIndex* suppliedPixels,
                                     std::size_t suppliedPixelSize) noexcept {
    *this = SpriteAssetPackView{};
    if (data == nullptr || suppliedPixels == nullptr || metadataSize < kHeaderSize
        || data[0] != 'P' || data[1] != 'T' || data[2] != 'S' || data[3] != 'P'
        || read16(data + 4) != 1 || read16(data + 6) != kHeaderSize
        || read16(data + 10) != kAssetSize) {
        return false;
    }

    const auto assetCount = read16(data + 8);
    const auto frameCount = read32(data + 12);
    const auto pixelDataOffset = read32(data + 16);
    const auto pixelDataSize = read32(data + 20);
    if (!multiplyFits(assetCount, kAssetSize, metadataSize - kHeaderSize)) {
        return false;
    }
    const auto assetTableEnd = kHeaderSize + static_cast<std::size_t>(assetCount) * kAssetSize;
    if (assetTableEnd > metadataSize
        || !multiplyFits(frameCount, kFrameSize, metadataSize - assetTableEnd)) {
        return false;
    }
    const auto frameTableEnd = assetTableEnd + static_cast<std::size_t>(frameCount) * kFrameSize;
    if (frameTableEnd > pixelDataOffset
        || pixelDataOffset - frameTableEnd > 3 || (pixelDataOffset & 3U) != 0
        || pixelDataOffset > metadataSize
        || pixelDataSize > suppliedPixelSize) {
        return false;
    }

    const auto* frameTable = data + assetTableEnd;
    for (std::uint16_t assetIndex = 0; assetIndex < assetCount; ++assetIndex) {
        const auto* asset = data + kHeaderSize + static_cast<std::size_t>(assetIndex) * kAssetSize;
        const auto firstFrame = read32(asset);
        const auto localFrameCount = read16(asset + 4);
        const auto columns = asset[6];
        const auto rows = asset[7];
        const auto logicalWidth = asset[8];
        const auto logicalHeight = asset[9];
        if (firstFrame > frameCount || localFrameCount > frameCount - firstFrame
            || columns == 0 || rows == 0
            || static_cast<std::uint16_t>(columns) * rows != localFrameCount
            || logicalWidth == 0 || logicalHeight == 0) {
            return false;
        }
        for (std::uint16_t localIndex = 0; localIndex < localFrameCount; ++localIndex) {
            const auto* frame = frameTable
                + (static_cast<std::size_t>(firstFrame) + localIndex) * kFrameSize;
            const auto pixelOffset = read32(frame);
            const auto trimX = frame[4];
            const auto trimY = frame[5];
            const auto width = frame[6];
            const auto height = frame[7];
            if (width == 0 || height == 0) {
                if (width != 0 || height != 0 || pixelOffset != kEmptyPixelOffset) {
                    return false;
                }
                continue;
            }
            const auto pixelCount = static_cast<std::uint32_t>(width) * height;
            if (pixelOffset == kEmptyPixelOffset || pixelOffset > pixelDataSize
                || pixelCount > pixelDataSize - pixelOffset
                || static_cast<unsigned>(trimX) + width > logicalWidth
                || static_cast<unsigned>(trimY) + height > logicalHeight) {
                return false;
            }
        }
    }

    data_ = data;
    frameTable_ = frameTable;
    pixelData_ = suppliedPixels;
    assetCount_ = assetCount;
    return true;
}

bool SpriteAssetPackView::valid() const noexcept {
    return data_ != nullptr;
}

std::uint16_t SpriteAssetPackView::assetCount() const noexcept {
    return assetCount_;
}

bool SpriteAssetPackView::assetInfo(std::uint16_t assetIndex,
                                    SpriteAssetInfo& result) const noexcept {
    if (!valid() || assetIndex >= assetCount_) {
        return false;
    }
    const auto* asset = data_ + kHeaderSize + static_cast<std::size_t>(assetIndex) * kAssetSize;
    result.frameCount = read16(asset + 4);
    result.columns = asset[6];
    result.rows = asset[7];
    result.logicalWidth = asset[8];
    result.logicalHeight = asset[9];
    return true;
}

bool SpriteAssetPackView::frame(std::uint16_t assetIndex,
                                std::uint16_t frameIndex,
                                SpriteFramePattern& result) const noexcept {
    if (!valid() || assetIndex >= assetCount_) {
        return false;
    }
    const auto* asset = data_ + kHeaderSize + static_cast<std::size_t>(assetIndex) * kAssetSize;
    const auto firstFrame = read32(asset);
    const auto localFrameCount = read16(asset + 4);
    if (frameIndex >= localFrameCount) {
        return false;
    }
    const auto* source = frameTable_
        + (static_cast<std::size_t>(firstFrame) + frameIndex) * kFrameSize;
    const auto pixelOffset = read32(source);
    result.trimX = source[4];
    result.trimY = source[5];
    result.width = source[6];
    result.height = source[7];
    result.pixels = pixelOffset == kEmptyPixelOffset ? nullptr : pixelData_ + pixelOffset;
    return true;
}

bool SpriteAssetPackView::frameAt(std::uint16_t assetIndex,
                                  std::uint8_t column,
                                  std::uint8_t row,
                                  SpriteFramePattern& result) const noexcept {
    SpriteAssetInfo info{};
    if (!assetInfo(assetIndex, info) || column >= info.columns || row >= info.rows) {
        return false;
    }
    const auto frameIndex = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(row) * info.columns + column);
    return frame(assetIndex, frameIndex, result);
}

bool SpriteAssetPackView::makeSprite(std::uint16_t assetIndex,
                                     std::uint16_t frameIndex,
                                     std::int16_t logicalX,
                                     std::int16_t logicalY,
                                     Sprite& result) const noexcept {
    SpriteFramePattern pattern{};
    if (!frame(assetIndex, frameIndex, pattern)) {
        return false;
    }
    const auto destinationX = static_cast<std::int32_t>(logicalX) + pattern.trimX;
    const auto destinationY = static_cast<std::int32_t>(logicalY) + pattern.trimY;
    if (destinationX > std::numeric_limits<std::int16_t>::max()
        || destinationY > std::numeric_limits<std::int16_t>::max()) {
        return false;
    }
    result = Sprite{static_cast<std::int16_t>(destinationX),
                    static_cast<std::int16_t>(destinationY),
                    pattern.width,
                    pattern.height,
                    pattern.pixels};
    return true;
}

bool SpriteAssetPackView::makeSpriteAt(std::uint16_t assetIndex,
                                       std::uint8_t column,
                                       std::uint8_t row,
                                       std::int16_t logicalX,
                                       std::int16_t logicalY,
                                       Sprite& result) const noexcept {
    SpriteAssetInfo info{};
    if (!assetInfo(assetIndex, info) || column >= info.columns || row >= info.rows) {
        return false;
    }
    const auto frameIndex = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(row) * info.columns + column);
    return makeSprite(assetIndex, frameIndex, logicalX, logicalY, result);
}

} // namespace pixel_twins
