#include "pixel_twins/rp2350/pwm_audio.hpp"

#include "pixel_twins/rp2350/board_pins.hpp"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/structs/pwm.h"
#include "pico/stdlib.h"

#include <cstddef>
#include <cstdint>

namespace pixel_twins::rp2350 {
namespace {

constexpr unsigned int kAudioDmaIrqIndex = 1;
constexpr unsigned int kAudioDmaIrq = DMA_IRQ_1;
// USBとPIO-USBの1msタイマー（既定0x80）には音声生成へ割り込ませる。
constexpr std::uint8_t kAudioIrqPriority = 0xc0;

[[nodiscard]] volatile std::uint16_t* pwmCompareAddress(
    unsigned int slice, unsigned int channel) noexcept {
    auto* compare = reinterpret_cast<volatile std::uint16_t*>(
        &pwm_hw->slice[slice].cc);
    return compare + channel;
}

[[nodiscard]] std::uint16_t pcmToPwm(std::int16_t sample,
                                    std::uint16_t top) noexcept {
    const auto unsignedSample =
        static_cast<std::uint32_t>(static_cast<std::int32_t>(sample) + 32768);
    return static_cast<std::uint16_t>(
        (unsignedSample * (static_cast<std::uint32_t>(top) + 1u)) >> 16u);
}

struct PwmBufferTarget {
    std::uint16_t* left;
    std::uint16_t* right;
    std::uint16_t top;
};

void writePwmFrame(void* context,
                   std::size_t frame,
                   std::int16_t left,
                   std::int16_t right) noexcept {
    auto& target = *static_cast<PwmBufferTarget*>(context);
    target.left[frame] = pcmToPwm(left, target.top);
    target.right[frame] = pcmToPwm(right, target.top);
}

} // namespace

PwmAudioPlayer* PwmAudioPlayer::instance_ = nullptr;

bool PwmAudioPlayer::initialize() noexcept {
    if (initialized_) return true;
    if (instance_ != nullptr) return false;

    constexpr auto leftPin = board::kAudioPwmLeftPin;
    constexpr auto rightPin = board::kAudioPwmRightPin;
    const auto leftSlice = pwm_gpio_to_slice_num(leftPin);
    const auto rightSlice = pwm_gpio_to_slice_num(rightPin);
    const auto leftChannel = pwm_gpio_to_channel(leftPin);
    const auto rightChannel = pwm_gpio_to_channel(rightPin);
    if (leftSlice == rightSlice) return false;

    const auto systemClock = clock_get_hz(clk_sys);
    const auto clocksPerSample = systemClock / kAudioSampleRate;
    if (clocksPerSample == 0 || clocksPerSample > 65536u) return false;
    pwmTop_ = static_cast<std::uint16_t>(clocksPerSample - 1u);

    gpio_set_function(leftPin, GPIO_FUNC_PWM);
    gpio_set_function(rightPin, GPIO_FUNC_PWM);

    auto pwmConfig = pwm_get_default_config();
    pwm_config_set_wrap(&pwmConfig, pwmTop_);
    pwm_init(leftSlice, &pwmConfig, false);
    pwm_init(rightSlice, &pwmConfig, false);
    const auto midpoint = static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(pwmTop_) + 1u) / 2u);
    pwm_set_chan_level(leftSlice, leftChannel, midpoint);
    pwm_set_chan_level(rightSlice, rightChannel, midpoint);
    pwm_set_counter(leftSlice, 0);
    pwm_set_counter(rightSlice, 0);

    for (std::size_t index = 0; index < kBufferCount; ++index) {
        leftDmaChannels_[index] = dma_claim_unused_channel(false);
        rightDmaChannels_[index] = dma_claim_unused_channel(false);
        if (leftDmaChannels_[index] < 0 || rightDmaChannels_[index] < 0) {
            return false;
        }
    }

    renderBuffer(0);
    renderBuffer(1);

    for (std::size_t index = 0; index < kBufferCount; ++index) {
        const auto next = (index + 1u) & 1u;
        configureDmaChannel(
            leftDmaChannels_[index], leftDmaChannels_[next],
            leftSlice, leftChannel, leftBuffers_[index]);
        configureDmaChannel(
            rightDmaChannels_[index], rightDmaChannels_[next],
            rightSlice, rightChannel, rightBuffers_[index]);
        dma_irqn_set_channel_enabled(
            kAudioDmaIrqIndex, rightDmaChannels_[index], true);
    }

    instance_ = this;
    irq_add_shared_handler(
        kAudioDmaIrq, dmaIrqHandler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_priority(kAudioDmaIrq, kAudioIrqPriority);
    irq_set_enabled(kAudioDmaIrq, true);

    initialized_ = true;
    dma_start_channel_mask(
        (1u << leftDmaChannels_[0]) | (1u << rightDmaChannels_[0]));
    pwm_set_mask_enabled((1u << leftSlice) | (1u << rightSlice));
    return true;
}

bool PwmAudioPlayer::playBgm(const Sequence& sequence) noexcept {
    return enqueue({CommandKind::PlayBgm, &sequence});
}

bool PwmAudioPlayer::stopBgm() noexcept {
    return enqueue({CommandKind::StopBgm, nullptr});
}

bool PwmAudioPlayer::stopAll() noexcept {
    return enqueue({CommandKind::StopAll, nullptr});
}

bool PwmAudioPlayer::playSfx(const SfxRequest& request) noexcept {
    return audioSystem_.playSfx(request);
}

bool PwmAudioPlayer::enqueue(Command command) noexcept {
    const auto write = commandWritePosition_.load(std::memory_order_relaxed);
    const auto read = commandReadPosition_.load(std::memory_order_acquire);
    if (write - read == kCommandCapacity) return false;
    commands_[write & kCommandIndexMask] = command;
    commandWritePosition_.store(write + 1u, std::memory_order_release);
    return true;
}

void PwmAudioPlayer::applyCommands() noexcept {
    auto read = commandReadPosition_.load(std::memory_order_relaxed);
    const auto write = commandWritePosition_.load(std::memory_order_acquire);
    while (read != write) {
        const auto command = commands_[read & kCommandIndexMask];
        switch (command.kind) {
        case CommandKind::PlayBgm:
            if (command.sequence != nullptr) audioSystem_.playBgm(*command.sequence);
            break;
        case CommandKind::StopBgm:
            audioSystem_.stopBgm();
            break;
        case CommandKind::StopAll:
            audioSystem_.stopAll();
            break;
        }
        ++read;
    }
    commandReadPosition_.store(read, std::memory_order_release);
}

void PwmAudioPlayer::renderBuffer(std::size_t bufferIndex) noexcept {
    const auto startedAt = time_us_32();
    applyCommands();
    PwmBufferTarget target{
        leftBuffers_[bufferIndex].data(),
        rightBuffers_[bufferIndex].data(),
        pwmTop_,
    };
    audioSystem_.renderFrames(&target, writePwmFrame);
    const auto elapsed = time_us_32() - startedAt;
    diagnostics_.latestRenderUs = elapsed;
    if (elapsed > diagnostics_.maximumRenderUs) diagnostics_.maximumRenderUs = elapsed;
    diagnostics_.totalRenderUs += elapsed;
}

void PwmAudioPlayer::configureDmaChannel(
    int channel,
    int chainTo,
    unsigned int pwmSlice,
    unsigned int pwmChannel,
    const PwmBuffer& buffer) noexcept {
    auto config = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(&config, PWM_DREQ_NUM(pwmSlice));
    channel_config_set_chain_to(&config, static_cast<unsigned int>(chainTo));
    dma_channel_configure(
        channel, &config, pwmCompareAddress(pwmSlice, pwmChannel),
        buffer.data(), buffer.size(), false);
}

void PwmAudioPlayer::handleDmaIrq() noexcept {
    for (std::size_t bufferIndex = 0; bufferIndex < kBufferCount; ++bufferIndex) {
        const auto rightChannel = rightDmaChannels_[bufferIndex];
        if (!dma_irqn_get_channel_status(kAudioDmaIrqIndex, rightChannel)) continue;
        dma_irqn_acknowledge_channel(kAudioDmaIrqIndex, rightChannel);

        const auto leftChannel = leftDmaChannels_[bufferIndex];
        while (dma_channel_is_busy(leftChannel)) tight_loop_contents();

        const auto playingIndex = (bufferIndex + 1u) & 1u;
        if (!dma_channel_is_busy(leftDmaChannels_[playingIndex])
            || !dma_channel_is_busy(rightDmaChannels_[playingIndex])) {
            ++diagnostics_.underruns;
        }

        renderBuffer(bufferIndex);
        dma_channel_set_read_addr(
            leftChannel, leftBuffers_[bufferIndex].data(), false);
        dma_channel_set_trans_count(leftChannel, kAudioBlockFrames, false);
        dma_channel_set_read_addr(
            rightChannel, rightBuffers_[bufferIndex].data(), false);
        dma_channel_set_trans_count(rightChannel, kAudioBlockFrames, false);
        ++diagnostics_.completedBlocks;
    }
}

void PwmAudioPlayer::dmaIrqHandler() {
    if (instance_ != nullptr) instance_->handleDmaIrq();
}

} // namespace pixel_twins::rp2350
