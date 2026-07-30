#include "pixel_twins/rp2350/sony_hid.hpp"

#include <climits>

namespace pixel_twins::rp2350 {
namespace {

constexpr std::uint16_t kSonyVendorId = 0x054c;
constexpr std::uint16_t kDualShock4ProductId1 = 0x05c4;
constexpr std::uint16_t kDualShock4ProductId2 = 0x09cc;
constexpr std::uint16_t kDualSenseProductId = 0x0ce6;
constexpr std::int16_t kAxisDeadzone = 8000;

void setButton(std::uint16_t& buttons, ControllerButton button, bool pressed) noexcept {
    if (pressed) {
        buttons = static_cast<std::uint16_t>(buttons | buttonMask(button));
    }
}

[[nodiscard]] std::int16_t decodeAxis(std::uint8_t value) noexcept {
    const auto expanded = static_cast<std::int32_t>(value) * 257 - 32768;
    if (expanded > -kAxisDeadzone && expanded < kAxisDeadzone) return 0;
    return static_cast<std::int16_t>(expanded);
}

void decodeHat(std::uint8_t hat, ControllerSample& sample) noexcept {
    const bool up = hat == 0 || hat == 1 || hat == 7;
    const bool right = hat == 1 || hat == 2 || hat == 3;
    const bool down = hat == 3 || hat == 4 || hat == 5;
    const bool left = hat == 5 || hat == 6 || hat == 7;

    setButton(sample.buttons, ControllerButton::dpadLeft, left);
    setButton(sample.buttons, ControllerButton::dpadUp, up);
    setButton(sample.buttons, ControllerButton::dpadRight, right);
    setButton(sample.buttons, ControllerButton::dpadDown, down);
    if (left != right) sample.x = left ? INT16_MIN : INT16_MAX;
    if (up != down) sample.y = up ? INT16_MIN : INT16_MAX;
}

void decodeButtons(std::uint8_t faceAndHat, std::uint8_t menu,
                   std::uint8_t system, ControllerSample& sample) noexcept {
    setButton(sample.buttons, ControllerButton::choiceLeft, (faceAndHat & 0x10u) != 0);
    setButton(sample.buttons, ControllerButton::choiceDown, (faceAndHat & 0x20u) != 0);
    setButton(sample.buttons, ControllerButton::choiceRight, (faceAndHat & 0x40u) != 0);
    setButton(sample.buttons, ControllerButton::choiceUp, (faceAndHat & 0x80u) != 0);
    setButton(sample.buttons, ControllerButton::back, (menu & 0x10u) != 0);
    setButton(sample.buttons, ControllerButton::start, (menu & 0x20u) != 0);
    setButton(sample.buttons, ControllerButton::system, (system & 0x01u) != 0);
    decodeHat(static_cast<std::uint8_t>(faceAndHat & 0x0fu), sample);
}

} // namespace

SonyControllerKind identifySonyController(
    std::uint16_t vendorId, std::uint16_t productId) noexcept {
    if (vendorId != kSonyVendorId) return SonyControllerKind::unsupported;
    if (productId == kDualShock4ProductId1 || productId == kDualShock4ProductId2) {
        return SonyControllerKind::dualShock4;
    }
    if (productId == kDualSenseProductId) return SonyControllerKind::dualSense;
    return SonyControllerKind::unsupported;
}

bool decodeSonyUsbReport(
    SonyControllerKind kind, const std::uint8_t* report, std::size_t length,
    ControllerSample& sample) noexcept {
    if (report == nullptr || length == 0 || report[0] != 1) return false;

    std::size_t leftX = 1;
    std::size_t leftY = 2;
    std::size_t faceAndHat = 0;
    std::size_t menu = 0;
    std::size_t system = 0;
    if (kind == SonyControllerKind::dualShock4) {
        if (length < 10) return false;
        faceAndHat = 5;
        menu = 6;
        system = 7;
    } else if (kind == SonyControllerKind::dualSense) {
        if (length < 11) return false;
        faceAndHat = 8;
        menu = 9;
        system = 10;
    } else {
        return false;
    }

    ControllerSample decoded{};
    decoded.connected = true;
    decoded.gamepad = true;
    decoded.x = decodeAxis(report[leftX]);
    decoded.y = decodeAxis(report[leftY]);
    decodeButtons(report[faceAndHat], report[menu], report[system], decoded);
    sample = decoded;
    return true;
}

} // namespace pixel_twins::rp2350
