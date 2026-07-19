#include "pixel_twins/sequencer.hpp"
#include "pixel_twins/sound_waves.hpp"

#include "test_check.hpp"

#include <array>
#include <cmath>

namespace {

using namespace pixel_twins;

void testStandardWaves() {
    check(kStandardWaves.sine.samples[0] == 0);
    check(kStandardWaves.sine.samples[8] == 32767);
    check(kStandardWaves.square.samples[15] == 28835);
    check(kStandardWaves.square.samples[16] == -28835);
    check(sizeof(kStandardWaves) == 576);
}

void testSequenceAndLoop() {
    const std::array<SequenceInstrument, 1> instruments{{
        SequenceInstrument{Timbre{&kStandardWaves.square,
                                  Envelope{0.0F, 0.0F, 1.0F, 0.0F},
                                  1.0F,
                                  -1.0F},
                           1.0F,
                           1500.0F,
                           1.0F,
                           0.0F},
    }};
    const std::array<SequenceEvent, 2> events{{
        SequenceEvent{0, 1, 60, 127, 0, 0, 0},
        SequenceEvent{1, 1, 60, 127, 0, 0, 0},
    }};
    const Sequence sequence{events.data(),
                            static_cast<std::uint32_t>(events.size()),
                            instruments.data(),
                            static_cast<std::uint8_t>(instruments.size()),
                            2,
                            1,
                            1,
                            true};
    Synthesizer synth;
    Sequencer sequencer;
    AudioBlock output{};

    sequencer.play(sequence, synth);
    check(sequencer.isPlaying());
    sequencer.advanceBlock(synth);
    check(synth.isVoiceActive(0));
    synth.renderBlock(output);
    check(output[0] == 28835);
    check(sequencer.blockPosition() == 1);

    sequencer.advanceBlock(synth);
    check(sequencer.blockPosition() == 1);
    synth.renderBlock(output);
    check(output[0] == 28835);

    sequencer.stop(synth);
    check(!sequencer.isPlaying());
    check(!synth.isVoiceActive(0));
}

void testOneShotStops() {
    const std::array<SequenceInstrument, 1> instruments{{
        SequenceInstrument{Timbre{&kStandardWaves.sine}, 1.0F, 0.0F, 1.0F, 0.0F},
    }};
    const std::array<SequenceEvent, 1> events{{SequenceEvent{0, 1, 69, 127, 0, 0, 0}}};
    const Sequence sequence{events.data(), 1, instruments.data(), 1, 1, 0, 0, false};
    Synthesizer synth;
    Sequencer sequencer;
    sequencer.play(sequence, synth);
    sequencer.advanceBlock(synth);
    check(sequencer.isPlaying());
    check(synth.isVoiceActive(0));
    AudioBlock output{};
    synth.renderBlock(output);
    sequencer.advanceBlock(synth);
    check(!sequencer.isPlaying());
    check(!synth.isVoiceActive(0));
}

void testTrackMuteMask() {
    const std::array<SequenceInstrument, 1> instruments{{
        SequenceInstrument{Timbre{&kStandardWaves.square,
                                  Envelope{0.0F, 0.0F, 1.0F, 0.0F}, 1.0F, -1.0F},
                           1.0F, 1500.0F, 1.0F, 0.0F},
    }};
    const std::array<SequenceEvent, 2> events{{
        SequenceEvent{0, 2, 60, 127, 0, 0, 0},
        SequenceEvent{1, 2, 60, 127, 0, 0, 1},
    }};
    const Sequence sequence{events.data(), 2, instruments.data(), 1, 3, 0, 0, false};
    Synthesizer synth;
    Sequencer sequencer;
    AudioBlock output{};

    sequencer.setTrackMuteMask(1U << 0U, synth);
    sequencer.play(sequence, synth);
    sequencer.advanceBlock(synth);
    synth.renderBlock(output);
    check(output[0] == 0);

    sequencer.setTrackMuteMask(0U, synth);
    sequencer.advanceBlock(synth);
    synth.renderBlock(output);
    check(output[0] == 28835);

    sequencer.setTrackMuteMask(1U << 1U, synth);
    check(!synth.isVoiceActive(0));
}

} // namespace

int main() {
    testStandardWaves();
    testSequenceAndLoop();
    testOneShotStops();
    testTrackMuteMask();
    return 0;
}
