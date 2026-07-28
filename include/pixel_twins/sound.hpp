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
inline constexpr std::size_t kWaveTableSize = 32;
inline constexpr std::size_t kExpandedWaveTableSize = 256;
inline constexpr std::size_t kWaveTableExpansion = kWaveTableSize / kWaveTableSourceSize;
inline constexpr float kAudioBlockSeconds =
    static_cast<float>(kAudioBlockFrames) / static_cast<float>(kAudioSampleRate);

using AudioBlock = std::array<std::int16_t, kAudioBlockFrames * kAudioChannels>;
using AudioFrameWriter = void (*)(
    void* context,
    std::size_t frame,
    std::int16_t left,
    std::int16_t right) noexcept;

using WaveTableSource = std::array<std::int16_t, kWaveTableSourceSize>;

template <std::size_t Size>
struct BasicWaveTable {
    static_assert(Size >= kWaveTableSourceSize);
    static_assert((Size & (Size - 1)) == 0);
    static_assert(Size % kWaveTableSourceSize == 0);

    std::array<std::int16_t, Size> samples{};

    constexpr BasicWaveTable() = default;

    constexpr explicit BasicWaveTable(const WaveTableSource& source) {
        constexpr auto expansion = Size / kWaveTableSourceSize;
        for (std::size_t i = 0; i < Size; ++i) {
            const auto sourceIndex = i / expansion;
            const auto fraction = i % expansion;
            const auto nextIndex = (sourceIndex + 1U) % kWaveTableSourceSize;
            const auto current = static_cast<std::int32_t>(source[sourceIndex]);
            const auto next = static_cast<std::int32_t>(source[nextIndex]);
            samples[i] = static_cast<std::int16_t>(
                (current * static_cast<std::int32_t>(expansion - fraction)
                 + next * static_cast<std::int32_t>(fraction))
                / static_cast<std::int32_t>(expansion));
        }
    }
};

using WaveTable = BasicWaveTable<kWaveTableSize>;
using ExpandedWaveTable = BasicWaveTable<kExpandedWaveTableSize>;

struct Waveform {
    const std::int16_t* samples = nullptr;
    std::uint8_t phaseShift = 0;
};

template <std::size_t Size>
[[nodiscard]] constexpr Waveform waveform(const BasicWaveTable<Size>& table) noexcept {
    auto bits = std::uint8_t{0};
    for (auto size = Size; size > 1; size >>= 1U) ++bits;
    return {table.samples.data(), static_cast<std::uint8_t>(32U - bits)};
}

[[nodiscard]] constexpr WaveTable makeNoiseWave(const WaveTableSource& source,
                                                std::uint32_t seed) noexcept {
    static_cast<void>(seed);
    return WaveTable{source};
}

struct Envelope {
    float attack = 0.0F;
    float decay = 0.0F;
    float sustain = 1.0F;
    float release = 0.0F;
};

struct Timbre {
    Waveform wave{};
    Envelope envelope{};
    float volume = 1.0F;
    float pan = 0.0F;

    constexpr Timbre() = default;

    template <std::size_t Size>
    constexpr Timbre(const BasicWaveTable<Size>* table,
                     Envelope requestedEnvelope = {},
                     float requestedVolume = 1.0F,
                     float requestedPan = 0.0F) noexcept
        : wave(table != nullptr ? waveform(*table) : Waveform{}),
          envelope(requestedEnvelope),
          volume(requestedVolume),
          pan(requestedPan) {}
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

struct NoiseStart {
    VoiceStart voice{};
    std::uint8_t priority = 0;
    float bodyVolume = 0.0F;
    float bodyFrequency = 0.0F;
    float bodyEndFrequency = 0.0F;
    float bodyPitchSeconds = 0.0F;
    float bodySeconds = 0.0F;
};

class Synthesizer {
public:
    void startVoice(std::size_t voice, const VoiceStart& start) noexcept;
    [[nodiscard]] bool startNoise(const NoiseStart& start) noexcept;
    void releaseVoice(std::size_t voice) noexcept;
    void stopVoice(std::size_t voice) noexcept;
    void stopNoise() noexcept;
    void stopAll() noexcept;

    void setMasterVolume(float volume) noexcept;
    [[nodiscard]] float masterVolume() const noexcept { return masterVolume_; }
    [[nodiscard]] bool isVoiceActive(std::size_t voice) const noexcept;
    [[nodiscard]] bool isNoiseActive() const noexcept { return noiseVoice_.voice.active; }

    void renderBlock(AudioBlock& output) noexcept;
    void renderFrames(void* context, AudioFrameWriter writer) noexcept;

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

    struct NoiseVoice {
        Voice voice{};
        std::uint32_t bodyPhase = 0;
        float bodyVolume = 0.0F;
        float bodyFrequency = 0.0F;
        float bodyEndFrequency = 0.0F;
        float bodyPitchSeconds = 0.0F;
        float bodySeconds = 0.0F;
        std::uint8_t priority = 0;
    };

    [[nodiscard]] static float envelopeBeforeRelease(const Voice& voice, float time) noexcept;
    static void advanceEnvelope(Voice& voice) noexcept;
    [[nodiscard]] static float envelopeLevel(Voice& voice) noexcept;
    [[nodiscard]] static float pitchAt(const Voice& voice, float time) noexcept;
    [[nodiscard]] static float bodyPitchAt(const NoiseVoice& voice, float time) noexcept;
    [[nodiscard]] static float bodyLevelAt(const NoiseVoice& voice, float time) noexcept;

    std::array<Voice, kAudioVoiceCount> voices_{};
    NoiseVoice noiseVoice_{};
    std::uint16_t noiseLfsr_ = 0x7fffU;
    float masterVolume_ = 1.0F;
};

static_assert(sizeof(WaveTable) == 64, "32要素の16bit波形は64バイトでなければなりません");
static_assert(sizeof(ExpandedWaveTable) == 512,
              "256要素の16bit波形は512バイトでなければなりません");

} // namespace pixel_twins
