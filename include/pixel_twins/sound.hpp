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
inline constexpr std::size_t kWaveTableSize = 32;
inline constexpr float kAudioBlockSeconds =
    static_cast<float>(kAudioBlockFrames) / static_cast<float>(kAudioSampleRate);

using AudioBlock = std::array<std::int16_t, kAudioBlockFrames * kAudioChannels>;

struct WaveTable {
    std::array<std::int16_t, kWaveTableSize> samples;
};

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

static_assert(sizeof(WaveTable) == 64, "32要素の16bit波形は64バイトでなければなりません");

} // namespace pixel_twins
