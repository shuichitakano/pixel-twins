#include "pixel_twins/rp2350/usb_controller.hpp"

#include "pixel_twins/rp2350/board_pins.hpp"
#include "pixel_twins/rp2350/sony_hid.hpp"

#include "pio_usb.h"
#include "tusb.h"

#include <algorithm>

namespace pixel_twins::rp2350 {
namespace {

constexpr std::uint8_t kUsbRootHubPort = 1;
constexpr std::uint8_t kUsbPio = 2;
constexpr std::uint8_t kUsbDmaChannel = 15;
UsbControllerInput* activeInput = nullptr;

} // namespace

bool UsbControllerInput::initialize() noexcept {
    if (initialized_) return true;
    if (activeInput != nullptr) return false;

    pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
    config.pin_dp = board::kUsb2DataPlusPin;
    config.pinout = PIO_USB_PINOUT_DPDM;
    config.pio_tx_num = kUsbPio;
    config.pio_rx_num = kUsbPio;
    config.sm_tx = 0;
    config.sm_rx = 1;
    config.sm_eop = 2;
    config.tx_ch = kUsbDmaChannel;

    activeInput = this;
    if (!tuh_configure(kUsbRootHubPort, TUH_CFGID_RPI_PIO_USB_CONFIGURATION,
                       &config)
        || !tuh_init(kUsbRootHubPort)) {
        activeInput = nullptr;
        return false;
    }
    initialized_ = true;
    return true;
}

void UsbControllerInput::task() noexcept {
    if (initialized_) tuh_task();
}

void UsbControllerInput::update(Controllers& controllers) noexcept {
    std::array<ControllerSample, kControllerCount> samples{};
    std::transform(slots_.begin(), slots_.end(), samples.begin(),
                   [](const Slot& slot) { return slot.sample; });
    controllers.update(samples);
}

void UsbControllerInput::mount(
    std::uint8_t deviceAddress, std::uint8_t instance,
    std::uint16_t vendorId, std::uint16_t productId) noexcept {
    const auto kind = identifySonyController(vendorId, productId);
    if (kind == SonyControllerKind::unsupported) return;

    const auto existing = std::find_if(
        slots_.begin(), slots_.end(), [=](const Slot& slot) {
            return slot.deviceAddress == deviceAddress && slot.instance == instance;
        });
    if (existing != slots_.end()) return;

    const auto free = std::find_if(slots_.begin(), slots_.end(),
                                   [](const Slot& slot) {
                                       return slot.deviceAddress == 0;
                                   });
    if (free == slots_.end()) return;
    free->deviceAddress = deviceAddress;
    free->instance = instance;
    free->kind = static_cast<std::uint8_t>(kind);
    free->sample.connected = true;
    free->sample.gamepad = true;
}

void UsbControllerInput::unmount(
    std::uint8_t deviceAddress, std::uint8_t instance) noexcept {
    const auto slot = std::find_if(
        slots_.begin(), slots_.end(), [=](const Slot& candidate) {
            return candidate.deviceAddress == deviceAddress
                && candidate.instance == instance;
        });
    if (slot != slots_.end()) *slot = {};
}

void UsbControllerInput::receive(
    std::uint8_t deviceAddress, std::uint8_t instance,
    const std::uint8_t* report, std::uint16_t length) noexcept {
    const auto slot = std::find_if(
        slots_.begin(), slots_.end(), [=](const Slot& candidate) {
            return candidate.deviceAddress == deviceAddress
                && candidate.instance == instance;
        });
    if (slot == slots_.end()) return;
    static_cast<void>(decodeSonyUsbReport(
        static_cast<SonyControllerKind>(slot->kind), report, length, slot->sample));
}

} // namespace pixel_twins::rp2350

extern "C" {

void tuh_hid_mount_cb(
    std::uint8_t devAddr, std::uint8_t instance,
    const std::uint8_t* reportDescriptor, std::uint16_t descriptorLength) {
    static_cast<void>(reportDescriptor);
    static_cast<void>(descriptorLength);
    if (pixel_twins::rp2350::activeInput == nullptr) return;

    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    tuh_vid_pid_get(devAddr, &vendorId, &productId);
    pixel_twins::rp2350::activeInput->mount(
        devAddr, instance, vendorId, productId);
    static_cast<void>(tuh_hid_receive_report(devAddr, instance));
}

void tuh_hid_umount_cb(std::uint8_t devAddr, std::uint8_t instance) {
    if (pixel_twins::rp2350::activeInput != nullptr) {
        pixel_twins::rp2350::activeInput->unmount(devAddr, instance);
    }
}

void tuh_hid_report_received_cb(
    std::uint8_t devAddr, std::uint8_t instance,
    const std::uint8_t* report, std::uint16_t length) {
    if (pixel_twins::rp2350::activeInput != nullptr) {
        pixel_twins::rp2350::activeInput->receive(
            devAddr, instance, report, length);
    }
    static_cast<void>(tuh_hid_receive_report(devAddr, instance));
}

} // extern "C"
