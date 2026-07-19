#pragma once

#include "pixel_twins/audio_system.hpp"

#include <atomic>

struct SDL_AudioStream;

namespace pixel_twins::sdl {

class AudioPlayer {
public:
    explicit AudioPlayer(AudioSystem& audioSystem);
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    [[nodiscard]] bool playBgm(const Sequence& sequence) noexcept;
    [[nodiscard]] bool stopBgm() noexcept;
    [[nodiscard]] bool stopAll() noexcept;
    [[nodiscard]] bool setMasterVolume(float volume) noexcept;
    [[nodiscard]] bool setBgmTrackMuteMask(std::uint8_t mask) noexcept;
    [[nodiscard]] bool healthy() const noexcept { return healthy_.load(std::memory_order_relaxed); }

private:
    static void audioCallback(void* userdata,
                              SDL_AudioStream* stream,
                              int additionalAmount,
                              int totalAmount) noexcept;
    [[nodiscard]] bool lock() noexcept;
    void unlock() noexcept;

    AudioSystem& audioSystem_;
    SDL_AudioStream* stream_ = nullptr;
    std::atomic<bool> healthy_{true};
};

} // namespace pixel_twins::sdl
