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
    if (request.voice.timbre == nullptr || request.voice.timbre->wave == nullptr) return false;
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
    SfxRequest discarded;
    while (sfxRequests_.tryPop(discarded)) {
    }
}

void AudioSystem::setMasterVolume(float volume) noexcept {
    synthesizer_.setMasterVolume(volume);
}

void AudioSystem::renderBlock(AudioBlock& output) noexcept {
    SfxRequest request;
    while (sfxRequests_.tryPop(request)) startSfx(request);
    sequencer_.advanceBlock(synthesizer_);
    synthesizer_.renderBlock(output);
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
