#include "pixel_twins/audio_system.hpp"

#include <cstddef>

namespace pixel_twins {

bool SfxRequestQueue::tryPush(const SfxRequest& request) noexcept {
    const auto write = writePosition_.load(std::memory_order_relaxed);
    const auto read = readPosition_.load(std::memory_order_acquire);
    if (write - read >= kSfxRequestCapacity) return false;
    requests_[write & kIndexMask] = request;
    writePosition_.store(write + 1, std::memory_order_release);
    return true;
}

bool SfxRequestQueue::tryPop(SfxRequest& request) noexcept {
    const auto read = readPosition_.load(std::memory_order_relaxed);
    const auto write = writePosition_.load(std::memory_order_acquire);
    if (read == write) return false;
    request = requests_[read & kIndexMask];
    readPosition_.store(read + 1, std::memory_order_release);
    return true;
}

bool AudioSystem::playSfx(const SfxRequest& request) noexcept {
    if (request.voice.timbre == nullptr || request.voice.timbre->wave == nullptr) {
        return false;
    }
    return sfxRequests_.tryPush(request);
}

void AudioSystem::playBgm(const Sequence& sequence) noexcept {
    sequencer_.play(sequence, synthesizer_);
}

void AudioSystem::stopBgm() noexcept {
    sequencer_.stop(synthesizer_);
}

void AudioSystem::stopAll() noexcept {
    sequencer_.stop(synthesizer_);
    synthesizer_.stopAll();
    for (auto& priority : sfxPriorities_) priority = 0;
    for (auto& age : sfxAges_) age = 0;
    for (auto& delayed : delayedSfx_) delayed = {};
    SfxRequest discarded;
    while (sfxRequests_.tryPop(discarded)) {
    }
}

void AudioSystem::setMasterVolume(float volume) noexcept {
    synthesizer_.setMasterVolume(volume);
}

void AudioSystem::setBgmTrackMuteMask(std::uint8_t mask) noexcept {
    sequencer_.setTrackMuteMask(mask, synthesizer_);
}

void AudioSystem::renderBlock(AudioBlock& output) noexcept {
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

void AudioSystem::renderFrames(void* context, AudioFrameWriter writer) noexcept {
    for (auto& delayed : delayedSfx_) {
        if (!delayed.active) continue;
        if (delayed.remainingBlocks > 0) --delayed.remainingBlocks;
        if (delayed.remainingBlocks == 0) {
            startSfx(delayed.request);
            delayed.active = false;
        }
    }
    SfxRequest request;
    while (sfxRequests_.tryPop(request)) scheduleSfx(request);
    sequencer_.advanceBlock(synthesizer_);
    synthesizer_.renderFrames(context, writer);
}

void AudioSystem::scheduleSfx(const SfxRequest& request) noexcept {
    if (request.delayBlocks == 0) {
        startSfx(request);
        return;
    }
    for (auto& delayed : delayedSfx_) {
        if (delayed.active) continue;
        delayed.request = request;
        delayed.remainingBlocks = request.delayBlocks;
        delayed.active = true;
        return;
    }
}

void AudioSystem::startSfx(const SfxRequest& request) noexcept {
    std::size_t selected = kSfxVoiceCount;
    for (std::size_t slot = 0; slot < kSfxVoiceCount; ++slot) {
        if (!synthesizer_.isVoiceActive(kBgmVoiceCount + slot)) {
            selected = slot;
            break;
        }
    }
    if (selected == kSfxVoiceCount) {
        selected = 0;
        for (std::size_t slot = 1; slot < kSfxVoiceCount; ++slot) {
            if (sfxPriorities_[slot] < sfxPriorities_[selected]
                || (sfxPriorities_[slot] == sfxPriorities_[selected]
                    && sfxAges_[slot] < sfxAges_[selected])) {
                selected = slot;
            }
        }
        if (request.priority < sfxPriorities_[selected]) return;
    }
    synthesizer_.startVoice(kBgmVoiceCount + selected, request.voice);
    sfxPriorities_[selected] = request.priority;
    sfxAges_[selected] = nextAge_++;
}

} // namespace pixel_twins
