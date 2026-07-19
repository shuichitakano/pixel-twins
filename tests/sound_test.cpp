#include "pixel_twins/sound.hpp"

#include "test_check.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {

using namespace pixel_twins;

constexpr WaveTable makeRampWave() {
    WaveTableSource source{};
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = static_cast<std::int16_t>(i * 800);
    }
    return WaveTable{source};
}

constexpr WaveTable makeFullWave() {
    WaveTable wave{};
    for (auto& sample : wave.samples) sample = 32767;
    return wave;
}

constexpr auto kRampWave = makeRampWave();
constexpr auto kFullWave = makeFullWave();

void testSilenceOverwritesOutput() {
    Synthesizer synth;
    AudioBlock output;
    output.fill(123);
    synth.renderBlock(output);
    check(std::all_of(output.begin(), output.end(), [](auto sample) { return sample == 0; }));
}

void testWavePhaseAndHardPan() {
    const Timbre timbre{&kRampWave, Envelope{0.0F, 0.0F, 1.0F, 0.1F}, 1.0F, -1.0F};
    Synthesizer synth;
    synth.startVoice(0, VoiceStart{&timbre, 187.5F, 187.5F, 0.0F, 1.0F, 1.0F, 0.0F});
    AudioBlock output{};
    synth.renderBlock(output);
    for (std::size_t i = 0; i < kWaveTableSourceSize; ++i) {
        check(output[i * 2] == static_cast<std::int16_t>(i * 100));
        check(output[i * 2 + 1] == 0);
    }
}

void testPitchCurveOverridesScalarPitch() {
    constexpr std::array<float, 2> curve{{187.5F, 187.5F}};
    const Timbre timbre{&kRampWave, Envelope{0.0F, 0.0F, 1.0F, 0.1F}, 1.0F, -1.0F};
    Synthesizer synth;
    synth.startVoice(0, VoiceStart{&timbre, 0.0F, 0.0F, 0.1F, 1.0F, 1.0F, 0.0F,
                                   PitchCurve{curve.data(), 2}, 1.0F});
    AudioBlock output{};
    synth.renderBlock(output);
    for (std::size_t i = 0; i < kWaveTableSourceSize; ++i) {
        check(output[i * 2] == static_cast<std::int16_t>(i * 100));
        check(output[i * 2 + 1] == 0);
    }
}

void testNoiseExpansionPreservesSourceAnchors() {
    WaveTableSource source{};
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = static_cast<std::int16_t>(static_cast<std::int32_t>(i) * 1700 - 25000);
    }
    const auto linear = WaveTable{source};
    const auto noise = makeNoiseWave(source, 0x12345678U);
    bool hasAddedDetail = false;
    for (std::size_t i = 0; i < source.size(); ++i) {
        check(noise.samples[i * kWaveTableExpansion] == source[i]);
    }
    for (std::size_t i = 0; i < kWaveTableSize; ++i) {
        if (noise.samples[i] != linear.samples[i]) hasAddedDetail = true;
    }
    check(hasAddedDetail);
}

void testEnvelopeBreakpointsAreNotSkipped() {
    const Timbre timbre{&kFullWave, Envelope{0.001F, 0.001F, 0.25F, 0.001F}, 1.0F, -1.0F};
    Synthesizer synth;
    synth.startVoice(0, VoiceStart{&timbre, 440.0F, 440.0F, 0.0F, 1.0F, 1.0F, 0.0F});
    AudioBlock output{};

    synth.renderBlock(output);
    check(output[0] == 32767);
    check(output[0] == output[(kAudioBlockFrames - 1) * 2]);

    synth.renderBlock(output);
    check(std::abs(static_cast<int>(output[0]) - 8191) <= 1);
    check(output[0] == output[(kAudioBlockFrames - 1) * 2]);
}

void testSaturationAndStop() {
    const Timbre timbre{&kFullWave, Envelope{0.0F, 0.0F, 1.0F, 0.1F}, 1.0F, -1.0F};
    Synthesizer synth;
    for (std::size_t i = 0; i < kAudioVoiceCount; ++i) {
        synth.startVoice(i, VoiceStart{&timbre, 440.0F, 440.0F, 0.0F, 1.0F, 1.0F, 0.0F});
    }
    AudioBlock output{};
    synth.renderBlock(output);
    check(output[0] == 32767);
    synth.stopAll();
    synth.renderBlock(output);
    check(output[0] == 0);
}

} // namespace

int main() {
    testSilenceOverwritesOutput();
    testWavePhaseAndHardPan();
    testPitchCurveOverridesScalarPitch();
    testNoiseExpansionPreservesSourceAnchors();
    testEnvelopeBreakpointsAreNotSkipped();
    testSaturationAndStop();
    return 0;
}
