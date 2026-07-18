#include "pixel_twins/sequencer.hpp"

#include <algorithm>
#include <cmath>

namespace pixel_twins {

void Sequencer::play(const Sequence& sequence, Synthesizer& synthesizer) noexcept {
    stop(synthesizer);
    if (sequence.events == nullptr || sequence.instruments == nullptr || sequence.eventCount == 0
        || sequence.instrumentCount == 0 || sequence.endBlock == 0) {
        return;
    }
    sequence_ = &sequence;
    blockPosition_ = 0;
    eventIndex_ = 0;
    playing_ = true;
    finishPending_ = false;
}

void Sequencer::stop(Synthesizer& synthesizer) noexcept {
    for (std::size_t voice = 0; voice < kBgmVoiceCount; ++voice) synthesizer.stopVoice(voice);
    sequence_ = nullptr;
    blockPosition_ = 0;
    eventIndex_ = 0;
    playing_ = false;
    finishPending_ = false;
}

void Sequencer::advanceBlock(Synthesizer& synthesizer) noexcept {
    if (!playing_) return;
    if (finishPending_) {
        stop(synthesizer);
        return;
    }
    while (eventIndex_ < sequence_->eventCount) {
        const auto& event = sequence_->events[eventIndex_];
        if (event.block > blockPosition_) break;
        ++eventIndex_;
        if (event.block != blockPosition_ || event.voice >= kBgmVoiceCount
            || event.instrument >= sequence_->instrumentCount) {
            continue;
        }
        const auto& instrument = sequence_->instruments[event.instrument];
        const auto frequency = eventFrequency(instrument, event.note);
        const auto velocity = 0.24F + static_cast<float>(event.velocity) / 127.0F * 0.76F;
        synthesizer.startVoice(
            event.voice,
            VoiceStart{&instrument.timbre,
                       frequency,
                       frequency * std::max(0.0F, instrument.endRatio),
                       std::max(0.0F, instrument.pitchSeconds),
                       static_cast<float>(std::max<std::uint16_t>(1, event.durationBlocks))
                           * kAudioBlockSeconds,
                       velocity,
                       0.0F});
    }

    ++blockPosition_;
    if (blockPosition_ < sequence_->endBlock) return;
    if (sequence_->loop && sequence_->loopStartBlock < sequence_->endBlock
        && sequence_->loopEventIndex < sequence_->eventCount) {
        blockPosition_ = sequence_->loopStartBlock;
        eventIndex_ = sequence_->loopEventIndex;
        return;
    }
    finishPending_ = true;
}

float Sequencer::eventFrequency(const SequenceInstrument& instrument, std::uint8_t note) noexcept {
    if (instrument.fixedFrequency > 0.0F) return instrument.fixedFrequency;
    const auto semitones = (static_cast<float>(note) - 69.0F) / 12.0F;
    return 440.0F * std::exp2(semitones) * std::max(0.0F, instrument.pitchRatio);
}

} // namespace pixel_twins
