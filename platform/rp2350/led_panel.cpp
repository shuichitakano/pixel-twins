#include "pixel_twins/rp2350/led_panel.hpp"

#include "pixel_twins/rp2350/board_pins.hpp"

#include "led_panel.pio.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "pico/assert.h"
#include "pico/time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace pixel_twins::rp2350 {
namespace {

PIO const kDataPio = pio0;
PIO const kPwmPio = pio1;
constexpr unsigned int kCommandStateMachine = 0;
constexpr unsigned int kDataStateMachine = 1;
constexpr unsigned int kPwmStateMachine = 0;
constexpr unsigned int kDataDmaIrqIndex = 0;
constexpr unsigned int kDataDmaIrq = DMA_IRQ_0;
constexpr unsigned int kPwmIrq = PIO1_IRQ_0;
constexpr std::uint8_t kLedIrqPriority = 0x80;

constexpr std::size_t kPanelWidth = 160;
constexpr std::size_t kScanLines = 60;
constexpr std::size_t kDriverOutputCount = 16;
constexpr std::size_t kDriverCascadeCount = 10;
constexpr std::size_t kBitsPerPixel = 16;
constexpr std::size_t kBrightnessBits = 12;
constexpr std::uint32_t kDataClocksPerBlockMinusTwo =
    kDriverCascadeCount * kBitsPerPixel - 2;
constexpr std::uint32_t kOutputBlocksMinusOne = kDriverOutputCount - 1;
constexpr std::uint32_t kPwmClocksPerLine = 74;
constexpr std::uint32_t kPwmScansPerFrame = 35;

constexpr std::uint16_t kRedLaneMask =
    (1u << 0) | (1u << 1) | (1u << 6) | (1u << 7);
constexpr std::uint16_t kGreenLaneMask =
    (1u << 2) | (1u << 3) | (1u << 8) | (1u << 9);
constexpr std::uint16_t kBlueLaneMask =
    (1u << 4) | (1u << 5) | (1u << 10) | (1u << 11);

void waitForProgramIdle(PIO pio,
                        unsigned int stateMachine,
                        unsigned int programOffset) noexcept {
    while (!pio_sm_is_tx_fifo_empty(pio, stateMachine)) {
        tight_loop_contents();
    }
    while (pio_sm_get_pc(pio, stateMachine) != programOffset) {
        tight_loop_contents();
    }
}

template <std::size_t Capacity>
void putPackedSample(std::array<std::uint32_t, Capacity>& destination,
                     std::size_t firstWord,
                     std::size_t sampleIndex,
                     std::uint16_t sample) noexcept {
    const auto wordOffset = sampleIndex >> 1u;
    const auto wordShift = static_cast<unsigned int>((sampleIndex & 1u) * 12u);
    destination[firstWord + wordOffset] |=
        static_cast<std::uint32_t>(sample) << wordShift;
}

template <std::size_t Capacity>
bool appendCommand(std::array<std::uint32_t, Capacity>& destination,
                   std::size_t& usedWords,
                   std::size_t requestedClocks,
                   std::size_t highClocks,
                   bool hasData,
                   std::uint16_t red,
                   std::uint16_t green,
                   std::uint16_t blue) noexcept {
    const auto clockCount = (requestedClocks + 7u) & ~std::size_t{7};
    if (highClocks == 0 || highClocks >= clockCount) return false;

    const auto lowClocks = clockCount - highClocks;
    const auto packedWords = clockCount / 2u;
    if (usedWords + 1u + packedWords > destination.size()) return false;

    destination[usedWords++] =
        static_cast<std::uint32_t>(lowClocks - 1u) |
        (static_cast<std::uint32_t>(highClocks - 1u) << 16u);

    const auto firstWord = usedWords;
    std::fill_n(destination.data() + firstWord, packedWords, 0u);
    usedWords += packedWords;

    if (!hasData) return true;

    constexpr auto commandBits = kDriverCascadeCount * kBitsPerPixel;
    if (clockCount < commandBits) return false;
    const auto padding = clockCount - commandBits;

    for (std::size_t i = 0; i < commandBits; ++i) {
        const auto mask = static_cast<std::uint16_t>(
            1u << (kBitsPerPixel - 1u - (i % kBitsPerPixel)));
        std::uint16_t sample = 0;
        if ((red & mask) != 0) sample |= kRedLaneMask;
        if ((green & mask) != 0) sample |= kGreenLaneMask;
        if ((blue & mask) != 0) sample |= kBlueLaneMask;
        putPackedSample(destination, firstWord, padding + i, sample);
    }
    return true;
}

template <std::size_t Capacity>
bool appendSimpleCommand(std::array<std::uint32_t, Capacity>& destination,
                         std::size_t& usedWords,
                         std::size_t lowClocks,
                         std::size_t highClocks) noexcept {
    return appendCommand(
        destination, usedWords, lowClocks + highClocks, highClocks, false, 0, 0, 0);
}

[[nodiscard]] std::uint16_t gammaTo12Bit(std::uint8_t value) noexcept {
    const auto normalized = static_cast<float>(value) / 255.0f;
    const auto corrected = std::pow(normalized, 2.2f);
    return static_cast<std::uint16_t>(corrected * 4095.0f + 0.5f);
}

template <std::size_t Size>
void setSequenceLane(std::array<std::uint32_t, Size>& sequence,
                     std::size_t sample,
                     unsigned int lane) noexcept {
    const auto word = sample >> 1u;
    const auto shift = static_cast<unsigned int>((sample & 1u) * 12u) + lane;
    sequence[word] |= 1u << shift;
}

void configureOutputPins(PIO pio, unsigned int stateMachine) noexcept {
    pio_sm_set_consecutive_pindirs(
        pio, stateMachine, board::kLedDataPinBase, board::kLedDataPinCount, true);
    pio_sm_set_consecutive_pindirs(
        pio, stateMachine, board::kLedClockPin, 2, true);
}

void configurePinElectricalCharacteristics() noexcept {
    for (unsigned int pin = board::kLedDataPinBase;
         pin < board::kLedDataPinBase + board::kLedDataPinCount;
         ++pin) {
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_8MA);
    }

    for (unsigned int pin = board::kLedClockPin;
         pin <= board::kLedOutputEnablePin;
         ++pin) {
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA);
    }
    gpio_set_drive_strength(board::kLedRowBPin, GPIO_DRIVE_STRENGTH_8MA);
}

} // namespace

LedPanelDriver* LedPanelDriver::instance_ = nullptr;

LedPanelDriver::LedPanelDriver() noexcept
    : command0Size_(0),
      command1Size_(0),
      dataDmaChannel_(-1),
      commandDmaChannel_(-1),
      pwmDmaChannel_(-1),
      commandProgramOffset_(0),
      dataProgramOffset_(0),
      pwmProgramOffset_(0),
      lastPresentActiveUs_(0),
      totalPresentActiveUs_(0),
      presentingPixels_(nullptr),
      nextTransferLine_(0),
      nextBuildLine_(0),
      presentActiveUs_(0),
      dataTransferComplete_(false),
      pwmScanComplete_(false),
      presenting_(false),
      initialized_(false) {}

void LedPanelDriver::initialize() noexcept {
    if (initialized_) return;
    hard_assert(instance_ == nullptr);

    commandProgramOffset_ = pio_add_program(kDataPio, &led_command_12_program);
    dataProgramOffset_ = pio_add_program(kDataPio, &led_data_12_program);
    pwmProgramOffset_ = pio_add_program(kPwmPio, &led_pwm_program);

    for (unsigned int pin = board::kLedDataPinBase;
         pin <= board::kLedLatchPin;
         ++pin) {
        pio_gpio_init(kDataPio, pin);
    }
    for (unsigned int pin = board::kLedRowAPin;
         pin <= board::kLedOutputEnablePin;
         ++pin) {
        pio_gpio_init(kPwmPio, pin);
    }

    const auto pioClockDivider =
        static_cast<float>(clock_get_hz(clk_sys)) / 25'000'000.0f;

    {
        auto config = led_command_12_program_get_default_config(commandProgramOffset_);
        sm_config_set_out_pins(
            &config, board::kLedDataPinBase, board::kLedDataPinCount);
        sm_config_set_sideset_pins(&config, board::kLedClockPin);
        sm_config_set_out_shift(&config, true, true, 24);
        sm_config_set_clkdiv(&config, pioClockDivider);
        pio_sm_init(kDataPio, kCommandStateMachine, commandProgramOffset_, &config);
        configureOutputPins(kDataPio, kCommandStateMachine);
    }

    {
        auto config = led_data_12_program_get_default_config(dataProgramOffset_);
        sm_config_set_out_pins(
            &config, board::kLedDataPinBase, board::kLedDataPinCount);
        sm_config_set_sideset_pins(&config, board::kLedClockPin);
        sm_config_set_out_shift(&config, true, true, 24);
        sm_config_set_clkdiv(&config, pioClockDivider);
        pio_sm_init(kDataPio, kDataStateMachine, dataProgramOffset_, &config);
        configureOutputPins(kDataPio, kDataStateMachine);

        pio_sm_put_blocking(kDataPio, kDataStateMachine, kDataClocksPerBlockMinusTwo);
        pio_sm_exec(kDataPio, kDataStateMachine, pio_encode_pull(false, false));
        pio_sm_exec(kDataPio, kDataStateMachine, pio_encode_out(pio_isr, 32));
        pio_sm_clear_fifos(kDataPio, kDataStateMachine);
    }

    {
        auto config = led_pwm_program_get_default_config(pwmProgramOffset_);
        sm_config_set_set_pins(&config, board::kLedRowAPin, 3);
        sm_config_set_sideset_pins(&config, board::kLedOutputEnablePin);
        sm_config_set_out_shift(&config, true, true, 32);
        sm_config_set_clkdiv(&config, pioClockDivider);
        pio_sm_init(kPwmPio, kPwmStateMachine, pwmProgramOffset_, &config);
        pio_sm_set_consecutive_pindirs(
            kPwmPio, kPwmStateMachine, board::kLedRowAPin, 3, true);
        pio_sm_set_consecutive_pindirs(
            kPwmPio, kPwmStateMachine, board::kLedOutputEnablePin, 1, true);
        // 最初のPWMワードが来るまではラインドライバ出力を無効にする。
        pio_sm_exec(
            kPwmPio, kPwmStateMachine, pio_encode_set(pio_pins, 0b010));
        pio_sm_set_enabled(kPwmPio, kPwmStateMachine, true);
    }

    configurePinElectricalCharacteristics();

    dataDmaChannel_ = dma_claim_unused_channel(true);
    commandDmaChannel_ = dma_claim_unused_channel(true);
    pwmDmaChannel_ = dma_claim_unused_channel(true);

    command0Size_ = 0;
    hard_assert(appendSimpleCommand(command0_, command0Size_, 1, 14));
    hard_assert(appendSimpleCommand(command0_, command0Size_, 8, 12));
    hard_assert(appendSimpleCommand(command0_, command0Size_, 8, 3));
    hard_assert(appendSimpleCommand(command0_, command0Size_, 8, 14));

    command1Size_ = 0;
    hard_assert(appendCommand(
        command1_, command1Size_, 168, 4, true, 0x3b70, 0x3b70, 0x3b70));
    hard_assert(appendSimpleCommand(command1_, command1Size_, 8, 14));
    hard_assert(appendCommand(
        command1_, command1Size_, 168, 6, true, 0x7f35, 0x6735, 0x5f35));
    hard_assert(appendSimpleCommand(command1_, command1Size_, 8, 14));
    hard_assert(appendCommand(
        command1_, command1Size_, 168, 8, true, 0x40f7, 0x40f7, 0x40f7));
    hard_assert(appendSimpleCommand(command1_, command1Size_, 8, 14));
    hard_assert(appendCommand(
        command1_, command1Size_, 168, 10, true, 0x0000, 0x0000, 0x0000));
    hard_assert(appendSimpleCommand(command1_, command1Size_, 8, 14));
    hard_assert(appendCommand(
        command1_, command1Size_, 168, 2, true, 0x0000, 0x0000, 0x0000));

    const auto pwmWord =
        ((static_cast<std::uint32_t>(kScanLines) - 2u) << 16u) |
        (kPwmClocksPerLine - 1u);
    std::fill_n(pwmWords_.begin(), kPwmScansPerFrame, pwmWord);
    pwmWords_.back() = 0;

    instance_ = this;
    dma_irqn_set_channel_enabled(
        kDataDmaIrqIndex, dataDmaChannel_, true);
    irq_add_shared_handler(
        kDataDmaIrq, dmaIrqHandler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_priority(kDataDmaIrq, kLedIrqPriority);
    irq_set_enabled(kDataDmaIrq, true);

    pio_interrupt_clear(kPwmPio, 0);
    pio_set_irq0_source_enabled(kPwmPio, pis_interrupt0, true);
    irq_add_shared_handler(
        kPwmIrq, pwmIrqHandler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_priority(kPwmIrq, kLedIrqPriority);
    irq_set_enabled(kPwmIrq, true);

    initialized_ = true;
}

void LedPanelDriver::setPalette(const Palette& palette) noexcept {
    for (std::size_t color = 0; color < palette.size(); ++color) {
        auto& sequence = colorSequences_[color];
        sequence.fill(0);

        const auto red = gammaTo12Bit(palette[color].r);
        const auto green = gammaTo12Bit(palette[color].g);
        const auto blue = gammaTo12Bit(palette[color].b);

        for (std::size_t bit = 0; bit < kBrightnessBits; ++bit) {
            const auto mask = static_cast<std::uint16_t>(
                1u << (kBrightnessBits - 1u - bit));
            const auto sample = 4u + bit;
            if ((red & mask) != 0) setSequenceLane(sequence, sample, 0u);
            if ((green & mask) != 0) setSequenceLane(sequence, sample, 2u);
            if ((blue & mask) != 0) setSequenceLane(sequence, sample, 4u);
        }
    }
}

void LedPanelDriver::buildLineBuffer(LineBuffer& destination,
                                     const PixelBuffer& pixels,
                                     std::size_t scanLine) noexcept {
    destination[0] = kOutputBlocksMinusOne;
    auto destinationWord = std::size_t{1};

    const auto upperRow = scanLine * kScreenWidth;
    const auto lowerRow = (scanLine + kScanLines) * kScreenWidth;
    for (std::size_t output = 0; output < kDriverOutputCount; ++output) {
        for (std::size_t cascade = 0; cascade < kDriverCascadeCount; ++cascade) {
            const auto x = cascade * kDriverOutputCount + output;
            const auto& panel1Upper = colorSequences_[pixels[upperRow + x]];
            const auto& panel1Lower = colorSequences_[pixels[lowerRow + x]];
            const auto& panel2Upper =
                colorSequences_[pixels[upperRow + kPanelWidth + x]];
            const auto& panel2Lower =
                colorSequences_[pixels[lowerRow + kPanelWidth + x]];

            for (std::size_t word = 0; word < kSequenceWords; ++word) {
                destination[destinationWord++] =
                    panel1Upper[word] |
                    (panel1Lower[word] << 1u) |
                    (panel2Upper[word] << 6u) |
                    (panel2Lower[word] << 7u);
            }
        }
    }
    hard_assert(destinationWord == destination.size());
}

void LedPanelDriver::startDataTransfer(const LineBuffer& buffer) noexcept {
    auto config = dma_channel_get_default_config(dataDmaChannel_);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(
        &config, pio_get_dreq(kDataPio, kDataStateMachine, true));
    dma_channel_configure(
        dataDmaChannel_,
        &config,
        &kDataPio->txf[kDataStateMachine],
        buffer.data(),
        buffer.size(),
        true);
}

void LedPanelDriver::sendCommands() noexcept {
    const auto send = [this](const std::uint32_t* words, std::size_t size) {
        auto config = dma_channel_get_default_config(commandDmaChannel_);
        channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
        channel_config_set_read_increment(&config, true);
        channel_config_set_write_increment(&config, false);
        channel_config_set_dreq(
            &config, pio_get_dreq(kDataPio, kCommandStateMachine, true));
        dma_channel_configure(
            commandDmaChannel_,
            &config,
            &kDataPio->txf[kCommandStateMachine],
            words,
            size,
            true);
        dma_channel_wait_for_finish_blocking(commandDmaChannel_);
        waitForProgramIdle(
            kDataPio, kCommandStateMachine, commandProgramOffset_);
    };

    pio_sm_set_enabled(kDataPio, kCommandStateMachine, true);
    send(command0_.data(), command0Size_);
    send(command1_.data(), command1Size_);
    pio_sm_set_enabled(kDataPio, kCommandStateMachine, false);
}

void LedPanelDriver::startPwmScan() noexcept {
    auto config = dma_channel_get_default_config(pwmDmaChannel_);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(
        &config, pio_get_dreq(kPwmPio, kPwmStateMachine, true));
    dma_channel_configure(
        pwmDmaChannel_,
        &config,
        &kPwmPio->txf[kPwmStateMachine],
        pwmWords_.data(),
        pwmWords_.size(),
        true);
}

bool LedPanelDriver::startPresent(const PixelBuffer& pixels) noexcept {
    hard_assert(initialized_);
    if (presenting_) return false;

    const auto activeStartUs = time_us_32();
    sendCommands();
    buildLineBuffer(lineBuffers_[0], pixels, 0);

    presentingPixels_ = &pixels;
    nextTransferLine_ = 1;
    nextBuildLine_ = 2;
    presentActiveUs_ = time_us_32() - activeStartUs;
    totalPresentActiveUs_ += presentActiveUs_;
    dataTransferComplete_ = false;
    pwmScanComplete_ = false;
    presenting_ = true;

    pio_sm_set_enabled(kDataPio, kDataStateMachine, true);
    startPwmScan();
    startDataTransfer(lineBuffers_[0]);

    const auto secondLineStartUs = time_us_32();
    buildLineBuffer(lineBuffers_[1], pixels, 1);
    const auto secondLineUs = time_us_32() - secondLineStartUs;
    presentActiveUs_ += secondLineUs;
    totalPresentActiveUs_ += secondLineUs;
    return true;
}

void LedPanelDriver::handleDataDmaIrq() noexcept {
    if (!dma_irqn_get_channel_status(kDataDmaIrqIndex, dataDmaChannel_)) return;
    dma_irqn_acknowledge_channel(kDataDmaIrqIndex, dataDmaChannel_);
    if (!presenting_ || presentingPixels_ == nullptr) return;

    if (nextTransferLine_ < kScanLines) {
        const auto activeStartUs = time_us_32();
        startDataTransfer(lineBuffers_[nextTransferLine_ & 1u]);
        ++nextTransferLine_;
        if (nextBuildLine_ < kScanLines) {
            auto& freeBuffer = lineBuffers_[nextBuildLine_ & 1u];
            buildLineBuffer(freeBuffer, *presentingPixels_, nextBuildLine_);
            ++nextBuildLine_;
        }
        const auto activeUs = time_us_32() - activeStartUs;
        presentActiveUs_ += activeUs;
        totalPresentActiveUs_ += activeUs;
        return;
    }

    dataTransferComplete_ = true;
    finishPresentIfReady();
}

void LedPanelDriver::handlePwmIrq() noexcept {
    if (!pio_interrupt_get(kPwmPio, 0)) return;
    pio_interrupt_clear(kPwmPio, 0);
    if (!presenting_) return;
    pwmScanComplete_ = true;
    finishPresentIfReady();
}

void LedPanelDriver::finishPresentIfReady() noexcept {
    if (!dataTransferComplete_ || !pwmScanComplete_) return;
    pio_sm_set_enabled(kDataPio, kDataStateMachine, false);
    presentingPixels_ = nullptr;
    lastPresentActiveUs_ = presentActiveUs_;
    presenting_ = false;
}

void LedPanelDriver::dmaIrqHandler() {
    if (instance_ != nullptr) instance_->handleDataDmaIrq();
}

void LedPanelDriver::pwmIrqHandler() {
    if (instance_ != nullptr) instance_->handlePwmIrq();
}

void LedPanelDriver::present(const PixelBuffer& pixels) noexcept {
    while (!startPresent(pixels)) tight_loop_contents();
    while (presenting()) tight_loop_contents();
}

} // namespace pixel_twins::rp2350
