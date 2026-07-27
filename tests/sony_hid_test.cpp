#include "pixel_twins/rp2350/sony_hid.hpp"

#include <array>
#include <cassert>
#include <cstdint>

using namespace pixel_twins;
using namespace pixel_twins::rp2350;

namespace {

bool held(const ControllerSample& sample, ControllerButton button) {
    return (sample.buttons & buttonMask(button)) != 0;
}

void identifySupportedDevices() {
    assert(identifySonyController(0x054c, 0x05c4)
           == SonyControllerKind::dualShock4);
    assert(identifySonyController(0x054c, 0x09cc)
           == SonyControllerKind::dualShock4);
    assert(identifySonyController(0x054c, 0x0ce6)
           == SonyControllerKind::dualSense);
    assert(identifySonyController(0x054c, 0xffff)
           == SonyControllerKind::unsupported);
    assert(identifySonyController(0xffff, 0x0ce6)
           == SonyControllerKind::unsupported);
}

void decodeDualShock4() {
    // ID, LX, LY, RX, RY, hat+□×○△, L/R+Share/Options, PS/Touchpad, L2, R2
    const std::array<std::uint8_t, 10> report{
        1, 0, 255, 128, 128, 0xf1, 0x30, 0x02, 0, 0};
    ControllerSample sample{};
    assert(decodeSonyUsbReport(
        SonyControllerKind::dualShock4, report.data(), report.size(), sample));
    assert(sample.connected && sample.gamepad);
    assert(sample.x == INT16_MAX);
    assert(sample.y == INT16_MIN); // hat NEが左スティックより優先
    assert(held(sample, ControllerButton::dpadUp));
    assert(held(sample, ControllerButton::dpadRight));
    assert(held(sample, ControllerButton::choiceLeft));
    assert(held(sample, ControllerButton::choiceUp));
    assert(held(sample, ControllerButton::choiceRight));
    assert(held(sample, ControllerButton::choiceDown));
    assert(held(sample, ControllerButton::start));
    assert(held(sample, ControllerButton::back));
}

void decodeDualSense() {
    // ID, LX, LY, RX, RY, L2, R2, counter, hat+×, Options, neutral
    const std::array<std::uint8_t, 11> report{
        1, 128, 128, 0, 0, 0, 0, 0, 0x24, 0x20, 0};
    ControllerSample sample{};
    assert(decodeSonyUsbReport(
        SonyControllerKind::dualSense, report.data(), report.size(), sample));
    assert(sample.x == 0);
    assert(sample.y == INT16_MAX);
    assert(held(sample, ControllerButton::dpadDown));
    assert(held(sample, ControllerButton::choiceDown));
    assert(held(sample, ControllerButton::start));
    assert(!held(sample, ControllerButton::back));
}

void rejectInvalidReportsWithoutChangingSample() {
    const std::array<std::uint8_t, 2> shortReport{1, 128};
    const std::array<std::uint8_t, 11> wrongId{
        2, 128, 128, 0, 0, 0, 0, 0, 8, 0, 0};
    ControllerSample sample{};
    sample.buttons = 0x55aa;
    assert(!decodeSonyUsbReport(
        SonyControllerKind::dualSense, shortReport.data(), shortReport.size(), sample));
    assert(!decodeSonyUsbReport(
        SonyControllerKind::dualSense, wrongId.data(), wrongId.size(), sample));
    assert(sample.buttons == 0x55aa);
}

} // namespace

int main() {
    identifySupportedDevices();
    decodeDualShock4();
    decodeDualSense();
    rejectInvalidReportsWithoutChangingSample();
}
