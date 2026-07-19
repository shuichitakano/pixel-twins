#pragma once

#include "pixel_twins/sound.hpp"

#include <cstddef>
#include <cstdint>

namespace pixel_twins {

struct SequenceInstrument {
    Timbre timbre;
    float pitchRatio = 1.0F;
    float fixedFrequency = 0.0F;
    float endRatio = 1.0F;
    float pitchSeconds = 0.0F;
};

struct SequenceEvent {
    std::uint32_t block;
    std::uint16_t durationBlocks;
    std::uint8_t note;
    std::uint8_t velocity;
    std::uint8_t voice;
    std::uint8_t instrument;
};

static_assert(sizeof(SequenceEvent) == 12, "シーケンスイベントは12バイトでなければなりません");

struct Sequence {
    const SequenceEvent* events = nullptr;
    std::uint32_t eventCount = 0;
    const SequenceInstrument* instruments = nullptr;
    std::uint8_t instrumentCount = 0;
    std::uint32_t endBlock = 0;
    std::uint32_t loopStartBlock = 0;
    std::uint32_t loopEventIndex = 0;
    bool loop = false;
};

class Sequencer {
public:
    void play(const Sequence& sequence, Synthesizer& synthesizer) noexcept;
    void stop(Synthesizer& synthesizer) noexcept;
    void advanceBlock(Synthesizer& synthesizer) noexcept;
    void setVoiceMuteMask(std::uint8_t mask, Synthesizer& synthesizer) noexcept;

    [[nodiscard]] bool isPlaying() const noexcept { return playing_; }
    [[nodiscard]] std::uint32_t blockPosition() const noexcept { return blockPosition_; }
    [[nodiscard]] std::uint8_t voiceMuteMask() const noexcept { return voiceMuteMask_; }

private:
    [[nodiscard]] static float eventFrequency(const SequenceInstrument& instrument,
                                              std::uint8_t note) noexcept;

    const Sequence* sequence_ = nullptr;
    std::uint32_t blockPosition_ = 0;
    std::uint32_t eventIndex_ = 0;
    std::uint8_t voiceMuteMask_ = 0;
    bool playing_ = false;
    bool finishPending_ = false;
};

} // namespace pixel_twins
