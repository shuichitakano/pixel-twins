#pragma once

#include "pixel_twins/sequencer.hpp"
#include "pixel_twins/sound.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace pixel_twins {

inline constexpr std::size_t kSfxRequestCapacity = 32;

struct SfxRequest {
    VoiceStart voice;
    std::uint8_t priority = 0;
};

class SfxRequestQueue {
public:
    [[nodiscard]] bool tryPush(const SfxRequest& request) noexcept;
    [[nodiscard]] bool tryPop(SfxRequest& request) noexcept;

private:
    static constexpr std::size_t kIndexMask = kSfxRequestCapacity - 1;
    std::array<SfxRequest, kSfxRequestCapacity> requests_{};
    alignas(4) std::atomic<std::uint32_t> writePosition_{0};
    alignas(4) std::atomic<std::uint32_t> readPosition_{0};
};

class AudioSystem {
public:
    [[nodiscard]] bool playSfx(const SfxRequest& request) noexcept;
    void playBgm(const Sequence& sequence) noexcept;
    void stopBgm() noexcept;
    void stopAll() noexcept;
    void setMasterVolume(float volume) noexcept;
    void renderBlock(AudioBlock& output) noexcept PIXEL_TWINS_SRAM;

    [[nodiscard]] bool isBgmPlaying() const noexcept { return sequencer_.isPlaying(); }
    [[nodiscard]] Synthesizer& synthesizer() noexcept { return synthesizer_; }
    [[nodiscard]] const Synthesizer& synthesizer() const noexcept { return synthesizer_; }

private:
    void startSfx(const SfxRequest& request) noexcept;

    SfxRequestQueue sfxRequests_{};
    Synthesizer synthesizer_{};
    Sequencer sequencer_{};
    std::array<std::uint8_t, kSfxVoiceCount> sfxPriorities_{};
    std::array<std::uint32_t, kSfxVoiceCount> sfxAges_{};
    std::uint32_t nextAge_ = 1;
};

static_assert((kSfxRequestCapacity & (kSfxRequestCapacity - 1)) == 0,
              "SFXキュー容量は2の冪でなければなりません");
static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "SFXキューにはlock-freeな32bit atomicが必要です");

} // namespace pixel_twins
