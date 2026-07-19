#include "pixel_twins/sdl_audio.hpp"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace pixel_twins::sdl {
namespace {

[[nodiscard]] std::runtime_error sdlAudioError(const char* operation) {
    return std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

} // namespace

AudioPlayer::AudioPlayer(AudioSystem& audioSystem) : audioSystem_(audioSystem) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) throw sdlAudioError("SDL音声の初期化に失敗しました");
    const SDL_AudioSpec spec{SDL_AUDIO_S16, kAudioChannels, static_cast<int>(kAudioSampleRate)};
    stream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback, this);
    if (stream_ == nullptr) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        throw sdlAudioError("SDL音声デバイスを開けませんでした");
    }
    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        throw sdlAudioError("SDL音声デバイスを開始できませんでした");
    }
}

AudioPlayer::~AudioPlayer() {
    SDL_DestroyAudioStream(stream_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool AudioPlayer::playBgm(const Sequence& sequence) noexcept {
    if (!lock()) return false;
    audioSystem_.playBgm(sequence);
    unlock();
    return true;
}

bool AudioPlayer::stopBgm() noexcept {
    if (!lock()) return false;
    audioSystem_.stopBgm();
    unlock();
    return true;
}

bool AudioPlayer::stopAll() noexcept {
    if (!lock()) return false;
    audioSystem_.stopAll();
    unlock();
    return true;
}

bool AudioPlayer::setMasterVolume(float volume) noexcept {
    if (!lock()) return false;
    audioSystem_.setMasterVolume(volume);
    unlock();
    return true;
}

bool AudioPlayer::setBgmTrackMuteMask(std::uint8_t mask) noexcept {
    if (!lock()) return false;
    audioSystem_.setBgmTrackMuteMask(mask);
    unlock();
    return true;
}

void AudioPlayer::audioCallback(void* userdata,
                                SDL_AudioStream* stream,
                                int additionalAmount,
                                int) noexcept {
    auto& player = *static_cast<AudioPlayer*>(userdata);
    constexpr auto blockBytes = static_cast<int>(sizeof(AudioBlock));
    for (int supplied = 0; supplied < additionalAmount; supplied += blockBytes) {
        AudioBlock block;
        player.audioSystem_.renderBlock(block);
        if (!SDL_PutAudioStreamData(stream, block.data(), blockBytes)) {
            player.healthy_.store(false, std::memory_order_relaxed);
            return;
        }
    }
}

bool AudioPlayer::lock() noexcept {
    if (stream_ == nullptr || !SDL_LockAudioStream(stream_)) {
        healthy_.store(false, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void AudioPlayer::unlock() noexcept {
    SDL_UnlockAudioStream(stream_);
}

} // namespace pixel_twins::sdl
