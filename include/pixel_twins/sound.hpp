#pragma once

#include "pixel_twins/platform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace pixel_twins {

inline constexpr std::uint32_t kAudioSampleRate = 48000;
inline constexpr std::size_t kAudioBlockFrames = 512;
inline constexpr std::size_t kAudioChannels = 2;
inline constexpr std::size_t kAudioVoiceCount = 16;
inline constexpr std::size_t kBgmVoiceCount = 8;
inline constexpr std::size_t kSfxVoiceCount = 8;
inline constexpr std::size_t kWaveTableSourceSize = 32;
inline constexpr std::size_t kWaveTableSize = 256;
inline constexpr std::size_t kWaveTableExpansion = kWaveTableSize / kWaveTableSourceSize;
inline constexpr float kAudioBlockSeconds =
    static_cast<float>(kAudioBlockFrames) / static_cast<float>(kAudioSampleRate);

using AudioBlock = std::array<std::int16_t, kAudioBlockFrames * kAudioChannels>;

using WaveTableSource = std::array<std::int16_t, kWaveTableSourceSize>;

struct WaveTable {
    std::array<std::int16_t, kWaveTableSize> samples{};

    constexpr WaveTable() = default;

    constexpr explicit WaveTable(const WaveTableSource& source) {
        for (std::size_t i = 0; i < kWaveTableSize; ++i) {
            const auto sourceIndex = i / kWaveTableExpansion;
            const auto fraction = i % kWaveTableExpansion;
            const auto nextIndex = (sourceIndex + 1U) % kWaveTableSourceSize;
            const auto current = static_cast<std::int32_t>(source[sourceIndex]);
            const auto next = static_cast<std::int32_t>(source[nextIndex]);
            samples[i] = static_cast<std::int16_t>(
                (current * static_cast<std::int32_t>(kWaveTableExpansion - fraction)
                 + next * static_cast<std::int32_t>(fraction))
                / static_cast<std::int32_t>(kWaveTableExpansion));
        }
    }
};

[[nodiscard]] constexpr WaveTable makeNoiseWave(const WaveTableSource& source,
                                                std::uint32_t seed) noexcept {
    WaveTable result{source};
    auto random = seed != 0U ? seed : 1U;
    for (std::size_t i = 0; i < kWaveTableSize; ++i) {
        const auto fraction = i % kWaveTableExpansion;
        if (fraction == 0U) continue;
        random ^= random << 13U;
        random ^= random >> 17U;
        random ^= random << 5U;
        const auto sourceIndex = i / kWaveTableExpansion;
        const auto nextIndex = (sourceIndex + 1U) % kWaveTableSourceSize;
        const auto current = static_cast<std::int32_t>(source[sourceIndex]);
        const auto next = static_cast<std::int32_t>(source[nextIndex]);
        const auto difference = next >= current ? next - current : current - next;
        const auto amplitude = 2048 + difference / 5;
        const auto distanceToAnchor = fraction <= kWaveTableExpansion / 2U
            ? fraction
            : kWaveTableExpansion - fraction;
        const auto noise = static_cast<std::int32_t>((random >> 16U) & 0xffffU) - 32768;
        const auto jitter = static_cast<std::int32_t>(
            static_cast<std::int64_t>(noise) * amplitude
            * static_cast<std::int32_t>(distanceToAnchor)
            / (32768 * static_cast<std::int32_t>(kWaveTableExpansion / 2U)));
        const auto value = static_cast<std::int32_t>(result.samples[i]) + jitter;
        result.samples[i] = static_cast<std::int16_t>(
            value < -32768 ? -32768 : (value > 32767 ? 32767 : value));
    }
    return result;
}

struct Envelope {
    float attack = 0.0F;
    float decay = 0.0F;
    float sustain = 1.0F;
    float release = 0.0F;
};

struct Timbre {
    const WaveTable* wave = nullptr;
    Envelope envelope{};
    float volume = 1.0F;
    float pan = 0.0F;
};

struct PitchCurve {
    const float* frequencies = nullptr;
    std::uint8_t count = 0;
};

struct VoiceStart {
    const Timbre* timbre = nullptr;
    float frequency = 440.0F;
    float endFrequency = 440.0F;
    float pitchSeconds = 0.0F;
    float holdSeconds = 0.0F;
    float velocity = 1.0F;
    float pan = 0.0F;
    PitchCurve pitchCurve{};
    float pitchCurveScale = 1.0F;
};

class Synthesizer {
public:
    void startVoice(std::size_t voice, const VoiceStart& start) noexcept;
    void releaseVoice(std::size_t voice) noexcept;
    void stopVoice(std::size_t voice) noexcept;
    void stopAll() noexcept;

    void setMasterVolume(float volume) noexcept;
    [[nodiscard]] float masterVolume() const noexcept { return masterVolume_; }
    [[nodiscard]] bool isVoiceActive(std::size_t voice) const noexcept;

    void renderBlock(AudioBlock& output) noexcept PIXEL_TWINS_SRAM;

private:
    struct Voice {
        const Timbre* timbre = nullptr;
        std::uint32_t phase = 0;
        float frequency = 0.0F;
        float endFrequency = 0.0F;
        float pitchSeconds = 0.0F;
        float gateSeconds = 0.0F;
        float elapsed = 0.0F;
        float envelopeElapsed = 0.0F;
        float velocity = 0.0F;
        float pan = 0.0F;
        float releaseStart = 0.0F;
        float releaseLevel = 0.0F;
        PitchCurve pitchCurve{};
        float pitchCurveScale = 1.0F;
        bool envelopeStarted = false;
        bool releasing = false;
        bool active = false;
    };

    [[nodiscard]] static float envelopeBeforeRelease(const Voice& voice, float time) noexcept;
    static void advanceEnvelope(Voice& voice) noexcept;
    [[nodiscard]] static float envelopeLevel(Voice& voice) noexcept;
    [[nodiscard]] static float pitchAt(const Voice& voice, float time) noexcept;

    std::array<Voice, kAudioVoiceCount> voices_{};
    float masterVolume_ = 1.0F;
};

static_assert(kWaveTableSize % kWaveTableSourceSize == 0,
              "波形テーブルの展開倍率は整数でなければなりません");
static_assert(sizeof(WaveTable) == 512, "256要素の16bit波形は512バイトでなければなりません");

} // namespace pixel_twins
