#pragma once

#include "pixel_twins/audio_system.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace pixel_twins::rp2350 {

class PwmAudioPlayer {
public:
    struct Diagnostics {
        volatile std::uint32_t latestRenderUs = 0;
        volatile std::uint32_t maximumRenderUs = 0;
        volatile std::uint32_t totalRenderUs = 0;
        volatile std::uint32_t completedBlocks = 0;
        volatile std::uint32_t underruns = 0;
    };

    PwmAudioPlayer() noexcept = default;

    [[nodiscard]] bool initialize() noexcept;
    [[nodiscard]] bool playBgm(const Sequence& sequence) noexcept;
    [[nodiscard]] bool stopBgm() noexcept;
    [[nodiscard]] bool stopAll() noexcept;
    [[nodiscard]] bool playSfx(const SfxRequest& request) noexcept;

    [[nodiscard]] const Diagnostics& diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    enum class CommandKind : std::uint8_t {
        PlayBgm,
        StopBgm,
        StopAll,
    };

    struct Command {
        CommandKind kind = CommandKind::StopBgm;
        const Sequence* sequence = nullptr;
    };

    static constexpr std::size_t kBufferCount = 2;
    static constexpr std::size_t kCommandCapacity = 8;
    static constexpr std::size_t kCommandIndexMask = kCommandCapacity - 1;
    static_assert((kCommandCapacity & (kCommandCapacity - 1)) == 0,
                  "音声コマンドキュー容量は2の冪でなければなりません");

    using PwmBuffer = std::array<std::uint16_t, kAudioBlockFrames>;

    [[nodiscard]] bool enqueue(Command command) noexcept;
    void applyCommands() noexcept;
    void renderBuffer(std::size_t bufferIndex) noexcept;
    void configureDmaChannel(int channel,
                             int chainTo,
                             unsigned int pwmSlice,
                             unsigned int pwmChannel,
                             const PwmBuffer& buffer) noexcept;
    void handleDmaIrq() noexcept;
    static void dmaIrqHandler();

    static PwmAudioPlayer* instance_;

    AudioSystem audioSystem_{};
    std::array<PwmBuffer, kBufferCount> leftBuffers_{};
    std::array<PwmBuffer, kBufferCount> rightBuffers_{};
    std::array<int, kBufferCount> leftDmaChannels_{{-1, -1}};
    std::array<int, kBufferCount> rightDmaChannels_{{-1, -1}};
    std::array<Command, kCommandCapacity> commands_{};
    alignas(4) std::atomic<std::uint32_t> commandWritePosition_{0};
    alignas(4) std::atomic<std::uint32_t> commandReadPosition_{0};
    Diagnostics diagnostics_{};
    std::uint16_t pwmTop_ = 0;
    bool initialized_ = false;
};

} // namespace pixel_twins::rp2350
