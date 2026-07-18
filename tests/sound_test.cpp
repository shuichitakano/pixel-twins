#include "pixel_twins/sound.hpp"

#include "test_check.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {

using namespace pixel_twins;

constexpr WaveTable makeRampWave() {
    WaveTable wave{};
    for (std::size_t i = 0; i < wave.samples.size(); ++i) {
        wave.samples[i] = static_cast<std::int16_t>(i * 1000);
    }
    return wave;
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
    synth.startVoice(0, VoiceStart{&timbre, 1500.0F, 1500.0F, 0.0F, 1.0F, 1.0F, 0.0F});
    AudioBlock output{};
    synth.renderBlock(output);
    for (std::size_t i = 0; i < kWaveTableSize; ++i) {
        check(output[i * 2] == static_cast<std::int16_t>(i * 1000));
        check(output[i * 2 + 1] == 0);
    }
}

void testEnvelopeIsConstantWithinBlock() {
    const auto twoBlocks = kAudioBlockSeconds * 2.0F;
    const Timbre timbre{&kFullWave, Envelope{twoBlocks, 0.0F, 1.0F, twoBlocks}, 1.0F, -1.0F};
    Synthesizer synth;
    synth.startVoice(0, VoiceStart{&timbre, 440.0F, 440.0F, 0.0F, twoBlocks, 1.0F, 0.0F});
    AudioBlock output{};

    synth.renderBlock(output);
    check(output[0] == 0);
    check(output[(kAudioBlockFrames - 1) * 2] == 0);

    synth.renderBlock(output);
    check(std::abs(static_cast<int>(output[0]) - 16383) <= 1);
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
    testEnvelopeIsConstantWithinBlock();
    testSaturationAndStop();
    return 0;
}
