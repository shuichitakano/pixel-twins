#pragma once

#include "pixel_twins/controller.hpp"

#include <array>
#include <cstdint>

namespace pixel_twins::rp2350 {

class UsbControllerInput {
public:
    // GPIO20/21、PIO2、DMA15を使ってUSB2をホストとして初期化する。
    [[nodiscard]] bool initialize() noexcept;

    // core 0から高頻度に呼び、列挙とHID転送を進める。
    void task() noexcept;

    // 最新の2台分を共通コントローラー状態へ反映する。
    void update(Controllers& controllers) noexcept;

    // TinyUSB callbackからのみ使用する実装境界。
    void mount(std::uint8_t deviceAddress, std::uint8_t instance,
               std::uint16_t vendorId, std::uint16_t productId) noexcept;
    void unmount(std::uint8_t deviceAddress, std::uint8_t instance) noexcept;
    void receive(std::uint8_t deviceAddress, std::uint8_t instance,
                 const std::uint8_t* report, std::uint16_t length) noexcept;

private:
    struct Slot {
        ControllerSample sample{};
        std::uint8_t deviceAddress = 0;
        std::uint8_t instance = 0;
        std::uint8_t kind = 0;
    };

    std::array<Slot, kControllerCount> slots_{};
    bool initialized_ = false;
};

} // namespace pixel_twins::rp2350
