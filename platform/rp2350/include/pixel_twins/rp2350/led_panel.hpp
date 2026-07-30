#pragma once

#include "pixel_twins/framebuffer.hpp"
#include "pixel_twins/platform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace pixel_twins::rp2350 {

class LedPanelDriver {
public:
    LedPanelDriver() noexcept;

    void initialize() noexcept;
    void setPalette(const Palette& palette) noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool startPresent(const PixelBuffer& pixels) noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] bool presenting() const noexcept { return presenting_; }
    void present(const PixelBuffer& pixels) noexcept PIXEL_TWINS_SRAM;
    [[nodiscard]] std::uint32_t lastPresentActiveUs() const noexcept {
        return lastPresentActiveUs_;
    }
    [[nodiscard]] std::uint32_t totalPresentActiveUs() const noexcept {
        return totalPresentActiveUs_;
    }

private:
    static constexpr std::size_t kSequenceWords = 8;
    static constexpr std::size_t kLineDataWords = 16 * 10 * kSequenceWords;
    static constexpr std::size_t kLineBufferWords = 1 + kLineDataWords;
    static constexpr std::size_t kCommand0Capacity = 48;
    static constexpr std::size_t kCommand1Capacity = 512;
    static constexpr std::size_t kPwmWordCount = 2;

    using ColorSequence = std::array<std::uint32_t, kSequenceWords>;
    using LineBuffer = std::array<std::uint32_t, kLineBufferWords>;

    void buildLineBuffer(LineBuffer& destination,
                         const PixelBuffer& pixels,
                         std::size_t scanLine) noexcept PIXEL_TWINS_SRAM;
    void startDataTransfer(const LineBuffer& buffer) noexcept PIXEL_TWINS_SRAM;
    void sendCommands() noexcept PIXEL_TWINS_SRAM;
    void startPwmScan() noexcept PIXEL_TWINS_SRAM;
    void handleDataDmaIrq() noexcept PIXEL_TWINS_SRAM;
    void handlePwmIrq() noexcept PIXEL_TWINS_SRAM;
    static void dmaIrqHandler();
    static void pwmIrqHandler();

    static LedPanelDriver* instance_;
    std::array<ColorSequence, kPaletteSize> colorSequences_;
    std::array<LineBuffer, 2> lineBuffers_;
    std::array<std::uint32_t, kCommand0Capacity> command0_;
    std::array<std::uint32_t, kCommand1Capacity> command1_;
    std::size_t command0Size_;
    std::size_t command1Size_;
    int dataDmaChannel_;
    int commandDmaChannel_;
    int pwmDmaChannel_;
    std::array<std::uint32_t, kPwmWordCount> pwmWords_;
    std::uint32_t commandProgramOffset_;
    std::uint32_t dataProgramOffset_;
    std::uint32_t pwmProgramOffset_;
    std::uint32_t lastPresentActiveUs_;
    volatile std::uint32_t totalPresentActiveUs_;
    const PixelBuffer* presentingPixels_;
    std::size_t nextTransferLine_;
    std::size_t nextBuildLine_;
    std::uint32_t presentActiveUs_;
    volatile bool dataTransferComplete_;
    std::uint32_t pwmScansSinceFlip_;
    volatile bool presenting_;
    bool initialized_;
};

} // namespace pixel_twins::rp2350
