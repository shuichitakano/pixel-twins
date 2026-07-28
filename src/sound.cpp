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
    if (voiceIndex >= voices_.size() || start.timbre == nullptr
        || start.timbre->wave == nullptr) {
        return;
    }
    auto& voice = voices_[voiceIndex];
    voice = Voice{};
    voice.timbre = start.timbre;
    voice.frequency = std::max(0.0F, start.frequency);
    voice.endFrequency = start.endFrequency > 0.0F ? start.endFrequency : voice.frequency;
    voice.pitchSeconds = std::max(0.0F, start.pitchSeconds);
    voice.pitchCurve = start.pitchCurve;
    voice.pitchCurveScale = std::max(0.0F, start.pitchCurveScale);
    const auto& envelope = start.timbre->envelope;
    voice.gateSeconds = std::max(0.0F, envelope.attack) + std::max(0.0F, envelope.decay)
        + std::max(0.0F, start.holdSeconds);
    voice.velocity = clampUnit(start.velocity);
    voice.pan = std::clamp(start.timbre->pan + start.pan, -1.0F, 1.0F);
    voice.active = true;
}

bool Synthesizer::startNoise(const NoiseStart& start) noexcept {
    if (start.voice.timbre == nullptr || start.priority == 0
        || (noiseVoice_.voice.active && start.priority < noiseVoice_.priority)) return false;
    noiseVoice_ = NoiseVoice{};
    auto& voice = noiseVoice_.voice;
    voice.timbre = start.voice.timbre;
    const auto& envelope = start.voice.timbre->envelope;
    voice.gateSeconds = std::max(0.0F, envelope.attack) + std::max(0.0F, envelope.decay)
        + std::max(0.0F, start.voice.holdSeconds);
    voice.velocity = clampUnit(start.voice.velocity);
    voice.pan = std::clamp(start.voice.timbre->pan + start.voice.pan, -1.0F, 1.0F);
    voice.active = true;
    noiseVoice_.priority = start.priority;
    noiseVoice_.bodyVolume = std::max(0.0F, start.bodyVolume);
    noiseVoice_.bodyFrequency = std::max(0.0F, start.bodyFrequency);
    noiseVoice_.bodyEndFrequency = std::max(0.0F, start.bodyEndFrequency);
    noiseVoice_.bodyPitchSeconds = std::max(0.0F, start.bodyPitchSeconds);
    noiseVoice_.bodySeconds = std::max(0.0F, start.bodySeconds);
    return true;
}

void Synthesizer::releaseVoice(std::size_t voiceIndex) noexcept {
    if (voiceIndex >= voices_.size()) return;
    auto& voice = voices_[voiceIndex];
    if (!voice.active || voice.releasing) return;
    voice.releaseLevel = envelopeBeforeRelease(voice, voice.envelopeElapsed);
    voice.releaseStart = voice.envelopeElapsed;
    voice.releasing = true;
}

void Synthesizer::stopVoice(std::size_t voiceIndex) noexcept {
    if (voiceIndex < voices_.size()) voices_[voiceIndex] = Voice{};
}

void Synthesizer::stopNoise() noexcept {
    noiseVoice_ = NoiseVoice{};
}

void Synthesizer::stopAll() noexcept {
    for (auto& voice : voices_) voice = Voice{};
    stopNoise();
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

void Synthesizer::advanceEnvelope(Voice& voice) noexcept {
    const auto& envelope = voice.timbre->envelope;
    const auto attackEnd = std::max(0.0F, envelope.attack);
    const auto decayEnd = attackEnd + std::max(0.0F, envelope.decay);
    if (!voice.envelopeStarted) {
        voice.envelopeStarted = true;
        if (attackEnd <= 0.0F && decayEnd > 0.0F) return;
    }
    auto next = voice.envelopeElapsed + kAudioBlockSeconds;
    const auto stopAt = [&](float boundary) {
        if (boundary > voice.envelopeElapsed && boundary < next) next = boundary;
    };
    if (!voice.releasing) {
        stopAt(attackEnd);
        stopAt(decayEnd);
        stopAt(voice.gateSeconds);
    } else {
        stopAt(voice.releaseStart + std::max(0.0F, envelope.release));
    }
    voice.envelopeElapsed = next;
}

float Synthesizer::envelopeLevel(Voice& voice) noexcept {
    const auto release = std::max(0.0F, voice.timbre->envelope.release);
    if (!voice.releasing && voice.envelopeElapsed > voice.gateSeconds) {
        voice.releaseStart = voice.gateSeconds;
        voice.releaseLevel = envelopeBeforeRelease(voice, voice.gateSeconds);
        voice.releasing = true;
    }
    if (!voice.releasing) return envelopeBeforeRelease(voice, voice.envelopeElapsed);
    if (release <= 0.0F) {
        voice.active = false;
        return 0.0F;
    }
    const auto position = (voice.envelopeElapsed - voice.releaseStart) / release;
    if (position >= 1.0F) {
        voice.active = false;
        return 0.0F;
    }
    return voice.releaseLevel * (1.0F - clampUnit(position));
}

float Synthesizer::pitchAt(const Voice& voice, float time) noexcept {
    if (voice.pitchCurve.frequencies != nullptr && voice.pitchCurve.count >= 2
        && voice.pitchSeconds > 0.0F) {
        const auto position = clampUnit(time / voice.pitchSeconds)
            * static_cast<float>(voice.pitchCurve.count - 1U);
        const auto lower = static_cast<std::uint8_t>(position);
        const auto upper = static_cast<std::uint8_t>(
            std::min<unsigned>(lower + 1U, voice.pitchCurve.count - 1U));
        const auto fraction = position - static_cast<float>(lower);
        const auto frequency = voice.pitchCurve.frequencies[lower]
            + (voice.pitchCurve.frequencies[upper] - voice.pitchCurve.frequencies[lower])
                * fraction;
        return std::max(0.0F, frequency * voice.pitchCurveScale);
    }
    if (voice.pitchSeconds <= 0.0F || voice.frequency <= 0.0F || voice.endFrequency <= 0.0F
        || voice.frequency == voice.endFrequency) {
        return voice.frequency;
    }
    const auto position = clampUnit(time / voice.pitchSeconds);
    return voice.frequency * std::pow(voice.endFrequency / voice.frequency, position);
}

float Synthesizer::bodyPitchAt(const NoiseVoice& noise, float time) noexcept {
    if (noise.bodyPitchSeconds <= 0.0F || noise.bodyFrequency <= 0.0F
        || noise.bodyEndFrequency <= 0.0F || noise.bodyFrequency == noise.bodyEndFrequency) {
        return noise.bodyFrequency;
    }
    const auto position = clampUnit(time / noise.bodyPitchSeconds);
    return noise.bodyFrequency
        * std::pow(noise.bodyEndFrequency / noise.bodyFrequency, position);
}

float Synthesizer::bodyLevelAt(const NoiseVoice& noise, float time) noexcept {
    if (noise.bodySeconds <= 0.0F || time >= noise.bodySeconds) return 0.0F;
    constexpr float attack = 0.001F;
    constexpr float decayEnd = 0.025F;
    constexpr float sustain = 0.18F;
    if (time < attack) return time / attack;
    if (time < decayEnd) {
        return 1.0F + (sustain - 1.0F) * ((time - attack) / (decayEnd - attack));
    }
    return sustain * (1.0F - (time - decayEnd) / (noise.bodySeconds - decayEnd));
}

void Synthesizer::renderBlock(AudioBlock& output) noexcept {
    renderFrames(
        &output,
        [](void* context,
           std::size_t frame,
           std::int16_t left,
           std::int16_t right) noexcept {
            auto& block = *static_cast<AudioBlock*>(context);
            block[frame * kAudioChannels] = left;
            block[frame * kAudioChannels + 1u] = right;
        });
}

void Synthesizer::renderFrames(void* context, AudioFrameWriter writer) noexcept {
    if (writer == nullptr) return;
    struct BlockVoice {
        const WaveTable* wave;
        std::uint32_t increment;
        float left;
        float right;
        bool active;
    };
    std::array<BlockVoice, kAudioVoiceCount> blockVoices{};

    float noiseLeft = 0.0F;
    float noiseRight = 0.0F;
    float bodyLeft = 0.0F;
    float bodyRight = 0.0F;
    std::uint32_t bodyIncrement = 0;
    if (noiseVoice_.voice.active) {
        auto& voice = noiseVoice_.voice;
        advanceEnvelope(voice);
        const auto envelope = envelopeLevel(voice);
        if (voice.active && envelope > 0.0F) {
            const auto leftPan = std::sqrt(0.5F * (1.0F - voice.pan));
            const auto rightPan = std::sqrt(0.5F * (1.0F + voice.pan));
            const auto noiseGain = envelope * voice.velocity
                * std::max(0.0F, voice.timbre->volume) * masterVolume_;
            const auto bodyGain = bodyLevelAt(noiseVoice_, voice.envelopeElapsed) * voice.velocity
                * noiseVoice_.bodyVolume * masterVolume_;
            noiseLeft = noiseGain * leftPan;
            noiseRight = noiseGain * rightPan;
            bodyLeft = bodyGain * leftPan;
            bodyRight = bodyGain * rightPan;
            bodyIncrement = phaseIncrement(bodyPitchAt(noiseVoice_, voice.envelopeElapsed));
        }
    }

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        auto& voice = voices_[i];
        if (!voice.active) continue;
        advanceEnvelope(voice);
        const auto envelope = envelopeLevel(voice);
        if (!voice.active || envelope <= 0.0F) continue;
        const auto gain = envelope * voice.velocity * std::max(0.0F, voice.timbre->volume)
            * masterVolume_;
        const auto leftPan = std::sqrt(0.5F * (1.0F - voice.pan));
        const auto rightPan = std::sqrt(0.5F * (1.0F + voice.pan));
        blockVoices[i] = BlockVoice{
            voice.timbre->wave,
            phaseIncrement(pitchAt(voice, voice.elapsed)),
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
            const auto waveIndex = static_cast<std::size_t>(voice.phase >> 24U);
            const auto sample =
                static_cast<float>(block.wave->samples[waveIndex]) * 256.0F;
            left += sample * block.left;
            right += sample * block.right;
            voice.phase += block.increment;
        }
        if (noiseVoice_.voice.active) {
            const auto feedback = static_cast<std::uint16_t>(
                ((noiseLfsr_ >> 0U) ^ (noiseLfsr_ >> 1U)) & 1U);
            noiseLfsr_ = static_cast<std::uint16_t>((noiseLfsr_ >> 1U) | (feedback << 14U));
            const auto noiseSample = (noiseLfsr_ & 1U) != 0U ? 29490.0F : -29490.0F;
            const auto bodyPosition = static_cast<std::uint16_t>(noiseVoice_.bodyPhase >> 16U);
            const auto bodySample = bodyPosition < 32768U
                ? static_cast<float>(static_cast<std::int32_t>(bodyPosition) * 2 - 32767)
                : static_cast<float>(98303 - static_cast<std::int32_t>(bodyPosition) * 2);
            left += noiseSample * noiseLeft + bodySample * bodyLeft;
            right += noiseSample * noiseRight + bodySample * bodyRight;
            noiseVoice_.bodyPhase += bodyIncrement;
        }
        writer(context, frame, saturate16(left), saturate16(right));
    }

    for (auto& voice : voices_) {
        if (voice.active) voice.elapsed += kAudioBlockSeconds;
    }
    if (noiseVoice_.voice.active) noiseVoice_.voice.elapsed += kAudioBlockSeconds;
}

} // namespace pixel_twins
