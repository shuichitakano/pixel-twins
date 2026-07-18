#pragma once

#include "pixel_twins/platform.hpp"
#include "pixel_twins/render_target.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace pixel_twins {

inline constexpr std::uint16_t kInvalidSpriteIndex = 0xffff;
inline constexpr std::size_t kBucketCount = 120 + 60;

struct Sprite {
    std::int16_t dx;
    std::int16_t dy;
    std::uint8_t sw;
    std::uint8_t sh;
    std::uint16_t _next;
    const std::uint8_t* p;

    Sprite() noexcept = default;

    constexpr Sprite(std::int16_t destinationX,
                     std::int16_t destinationY,
                     std::uint8_t sourceWidth,
                     std::uint8_t sourceHeight,
                     const std::uint8_t* pattern) noexcept
        : dx(destinationX),
          dy(destinationY),
          sw(sourceWidth),
          sh(sourceHeight),
          _next(kInvalidSpriteIndex),
          p(pattern) {}
};

struct SpriteEx {
    std::int16_t dx;
    std::int16_t dy;
    std::uint8_t dw;
    std::uint8_t dh;
    std::uint8_t sw;
    std::uint8_t sh;
    std::uint16_t _next;
    const std::uint8_t* p;

    SpriteEx() noexcept = default;

    constexpr SpriteEx(std::int16_t destinationX,
                       std::int16_t destinationY,
                       std::uint8_t destinationWidth,
                       std::uint8_t destinationHeight,
                       std::uint8_t sourceWidth,
                       std::uint8_t sourceHeight,
                       const std::uint8_t* pattern) noexcept
        : dx(destinationX),
          dy(destinationY),
          dw(destinationWidth),
          dh(destinationHeight),
          sw(sourceWidth),
          sh(sourceHeight),
          _next(kInvalidSpriteIndex),
          p(pattern) {}
};

struct Bucket {
    std::uint16_t spriteTop;
    std::uint16_t spriteExTop;
};

#if UINTPTR_MAX == UINT32_MAX
static_assert(sizeof(Sprite) == 12, "RP2350上のSpriteは12バイトでなければなりません");
static_assert(sizeof(SpriteEx) == 16, "RP2350上のSpriteExは16バイトでなければなりません");
#endif

void drawSprite(RenderTarget target, const Sprite& sprite) noexcept PIXEL_TWINS_SRAM;
void drawSpriteEx(RenderTarget target, const SpriteEx& sprite) noexcept PIXEL_TWINS_SRAM;

template<typename T, std::size_t Capacity>
class FixedAllocator {
    static_assert(Capacity <= kInvalidSpriteIndex,
                  "固定長アロケーターの容量は65535以下でなければなりません");

public:
    PIXEL_TWINS_SRAM void reset() noexcept {
        count_ = 0;
    }

    [[nodiscard]] PIXEL_TWINS_SRAM std::uint16_t allocate(const T& value) noexcept {
        if (count_ >= Capacity) {
            return kInvalidSpriteIndex;
        }
        const auto index = count_++;
        entries_[index] = value;
        return index;
    }

    [[nodiscard]] PIXEL_TWINS_SRAM T& operator[](std::uint16_t index) noexcept {
        return entries_[index];
    }

    [[nodiscard]] PIXEL_TWINS_SRAM const T& operator[](std::uint16_t index) const noexcept {
        return entries_[index];
    }

    [[nodiscard]] PIXEL_TWINS_SRAM std::uint16_t size() const noexcept {
        return count_;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

private:
    std::array<T, Capacity> entries_;
    std::uint16_t count_ = 0;
};

template<std::size_t SpriteCapacity, std::size_t SpriteExCapacity>
class SpriteBuckets {
public:
    SpriteBuckets() noexcept {
        reset();
    }

    PIXEL_TWINS_SRAM void reset() noexcept {
        sprites_.reset();
        spritesEx_.reset();
        for (auto& bucket : buckets_) {
            bucket.spriteTop = kInvalidSpriteIndex;
            bucket.spriteExTop = kInvalidSpriteIndex;
        }
    }

    [[nodiscard]] PIXEL_TWINS_SRAM std::uint16_t addSprite(std::uint16_t bucketIndex,
                                                            const Sprite& sprite) noexcept {
        if (bucketIndex >= kBucketCount) {
            return kInvalidSpriteIndex;
        }
        const auto index = sprites_.allocate(sprite);
        if (index == kInvalidSpriteIndex) {
            return index;
        }
        sprites_[index]._next = buckets_[bucketIndex].spriteTop;
        buckets_[bucketIndex].spriteTop = index;
        return index;
    }

    [[nodiscard]] PIXEL_TWINS_SRAM std::uint16_t addSpriteEx(std::uint16_t bucketIndex,
                                                              const SpriteEx& sprite) noexcept {
        if (bucketIndex >= kBucketCount) {
            return kInvalidSpriteIndex;
        }
        const auto index = spritesEx_.allocate(sprite);
        if (index == kInvalidSpriteIndex) {
            return index;
        }
        spritesEx_[index]._next = buckets_[bucketIndex].spriteExTop;
        buckets_[bucketIndex].spriteExTop = index;
        return index;
    }

    PIXEL_TWINS_SRAM void draw(RenderTarget target) const noexcept {
        for (const auto& bucket : buckets_) {
            auto spriteIndex = bucket.spriteTop;
            while (spriteIndex != kInvalidSpriteIndex) {
                const auto& sprite = sprites_[spriteIndex];
                drawSprite(target, sprite);
                spriteIndex = sprite._next;
            }

            auto spriteExIndex = bucket.spriteExTop;
            while (spriteExIndex != kInvalidSpriteIndex) {
                const auto& sprite = spritesEx_[spriteExIndex];
                drawSpriteEx(target, sprite);
                spriteExIndex = sprite._next;
            }
        }
    }

    [[nodiscard]] PIXEL_TWINS_SRAM const FixedAllocator<Sprite, SpriteCapacity>&
    sprites() const noexcept {
        return sprites_;
    }

    [[nodiscard]] PIXEL_TWINS_SRAM const FixedAllocator<SpriteEx, SpriteExCapacity>&
    spritesEx() const noexcept {
        return spritesEx_;
    }

private:
    std::array<Bucket, kBucketCount> buckets_;
    FixedAllocator<Sprite, SpriteCapacity> sprites_;
    FixedAllocator<SpriteEx, SpriteExCapacity> spritesEx_;
};

} // namespace pixel_twins
