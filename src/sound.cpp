#include "pixel_twins/sound.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace pixel_twins {
namespace {

constexpr double kPhaseScale = 4294967296.0 / static_cast<double>(kAudioSampleRate);

[[nodiscard]] float clampUnit(float value) noexcept {
    return std::clamp(value, 0.0F, 1.0F);
}

[[nodiscard]] std::uint32_t phaseIncrement(float frequency) noexcept {
    const auto limited = std::clamp(frequency, 0.0F, static_cast<float>(kAudioSampleRate) * 0.5F);
    return static_cast<std::uint32_t>(static_cast<double>(limited) * kPhaseScale);
}

[[nodiscard]] std::int16_t saturate16(float value) noexcept {
    constexpr auto minimum = static_cast<float>(std::numeric_limits<std::int16_t>::min());
    constexpr auto maximum = static_cast<float>(std::numeric_limits<std::int16_t>::max());
    if (value <= minimum) return std::numeric_limits<std::int16_t>::min();
    if (value >= maximum) return std::numeric_limits<std::int16_t>::max();
    return static_cast<std::int16_t>(value);
}

} // namespace

void Synthesizer::startVoice(std::size_t voiceIndex, const VoiceStart& start) noexcept {
    if (voiceIndex >= voices_.size() || start.timbre == nullptr || start.timbre->wave == nullptr) {
        return;
    }
    auto& voice = voices_[voiceIndex];
    voice = Voice{};
    voice.timbre = start.timbre;
    voice.frequency = std::max(0.0F, start.frequency);
    voice.endFrequency = start.endFrequency > 0.0F ? start.endFrequency : voice.frequency;
    voice.pitchSeconds = std::max(0.0F, start.pitchSeconds);
    const auto& envelope = start.timbre->envelope;
    voice.gateSeconds = std::max(0.0F, envelope.attack) + std::max(0.0F, envelope.decay)
        + std::max(0.0F, start.holdSeconds);
    voice.velocity = clampUnit(start.velocity);
    voice.pan = std::clamp(start.timbre->pan + start.pan, -1.0F, 1.0F);
    voice.active = true;
}

void Synthesizer::releaseVoice(std::size_t voiceIndex) noexcept {
    if (voiceIndex >= voices_.size()) return;
    auto& voice = voices_[voiceIndex];
    if (!voice.active || voice.releasing) return;
    voice.releaseLevel = envelopeBeforeRelease(voice, voice.elapsed);
    voice.releaseStart = voice.elapsed;
    voice.releasing = true;
}

void Synthesizer::stopVoice(std::size_t voiceIndex) noexcept {
    if (voiceIndex < voices_.size()) voices_[voiceIndex] = Voice{};
}

void Synthesizer::stopAll() noexcept {
    for (auto& voice : voices_) voice = Voice{};
}

void Synthesizer::setMasterVolume(float volume) noexcept {
    masterVolume_ = clampUnit(volume);
}

bool Synthesizer::isVoiceActive(std::size_t voice) const noexcept {
    return voice < voices_.size() && voices_[voice].active;
}

float Synthesizer::envelopeBeforeRelease(const Voice& voice, float time) noexcept {
    const auto& envelope = voice.timbre->envelope;
    const auto attack = std::max(0.0F, envelope.attack);
    const auto decay = std::max(0.0F, envelope.decay);
    const auto sustain = clampUnit(envelope.sustain);
    if (attack > 0.0F && time < attack) return clampUnit(time / attack);
    const auto decayTime = time - attack;
    if (decay > 0.0F && decayTime < decay) {
        return 1.0F + (sustain - 1.0F) * clampUnit(decayTime / decay);
    }
    return sustain;
}

float Synthesizer::envelopeLevel(Voice& voice) noexcept {
    const auto release = std::max(0.0F, voice.timbre->envelope.release);
    if (!voice.releasing && voice.elapsed >= voice.gateSeconds) {
        voice.releaseStart = voice.gateSeconds;
        voice.releaseLevel = envelopeBeforeRelease(voice, voice.gateSeconds);
        voice.releasing = true;
    }
    if (!voice.releasing) return envelopeBeforeRelease(voice, voice.elapsed);
    if (release <= 0.0F) {
        voice.active = false;
        return 0.0F;
    }
    const auto position = (voice.elapsed - voice.releaseStart) / release;
    if (position >= 1.0F) {
        voice.active = false;
        return 0.0F;
    }
    return voice.releaseLevel * (1.0F - clampUnit(position));
}

float Synthesizer::pitchAt(const Voice& voice) noexcept {
    if (voice.pitchSeconds <= 0.0F || voice.frequency <= 0.0F || voice.endFrequency <= 0.0F
        || voice.frequency == voice.endFrequency) {
        return voice.frequency;
    }
    const auto position = clampUnit(voice.elapsed / voice.pitchSeconds);
    return voice.frequency * std::pow(voice.endFrequency / voice.frequency, position);
}

void Synthesizer::renderBlock(AudioBlock& output) noexcept {
    struct BlockVoice {
        const WaveTable* wave;
        std::uint32_t increment;
        float left;
        float right;
        bool active;
    };
    std::array<BlockVoice, kAudioVoiceCount> blockVoices{};

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        auto& voice = voices_[i];
        if (!voice.active) continue;
        const auto envelope = envelopeLevel(voice);
        if (!voice.active || envelope <= 0.0F) continue;
        const auto gain = envelope * voice.velocity * std::max(0.0F, voice.timbre->volume)
            * masterVolume_;
        const auto leftPan = std::sqrt(0.5F * (1.0F - voice.pan));
        const auto rightPan = std::sqrt(0.5F * (1.0F + voice.pan));
        blockVoices[i] = BlockVoice{
            voice.timbre->wave,
            phaseIncrement(pitchAt(voice)),
            gain * leftPan,
            gain * rightPan,
            true,
        };
    }

    for (std::size_t frame = 0; frame < kAudioBlockFrames; ++frame) {
        float left = 0.0F;
        float right = 0.0F;
        for (std::size_t i = 0; i < voices_.size(); ++i) {
            const auto& block = blockVoices[i];
            if (!block.active) continue;
            auto& voice = voices_[i];
            const auto waveIndex = static_cast<std::size_t>(voice.phase >> 27U);
            const auto sample = static_cast<float>(block.wave->samples[waveIndex]);
            left += sample * block.left;
            right += sample * block.right;
            voice.phase += block.increment;
        }
        output[frame * 2] = saturate16(left);
        output[frame * 2 + 1] = saturate16(right);
    }

    for (auto& voice : voices_) {
        if (voice.active) voice.elapsed += kAudioBlockSeconds;
    }
}

} // namespace pixel_twins
