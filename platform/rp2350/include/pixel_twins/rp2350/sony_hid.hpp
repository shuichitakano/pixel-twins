#pragma once

#include "pixel_twins/controller.hpp"

#include <cstddef>
#include <cstdint>

namespace pixel_twins::rp2350 {

enum class SonyControllerKind : std::uint8_t {
    unsupported,
    dualShock4,
    dualSense,
};

[[nodiscard]] SonyControllerKind identifySonyController(
    std::uint16_t vendorId, std::uint16_t productId) noexcept;

// USB接続時の入力レポートを共通ControllerSampleへ変換する。
// Bluetoothレポートは先頭形式が異なるため、この関数では受け付けない。
[[nodiscard]] bool decodeSonyUsbReport(
    SonyControllerKind kind, const std::uint8_t* report, std::size_t length,
    ControllerSample& sample) noexcept;

} // namespace pixel_twins::rp2350
