#include "pixel_twins/rp2350/usb_controller.hpp"

#include "pixel_twins/rp2350/board_pins.hpp"
#include "pixel_twins/rp2350/sony_hid.hpp"

#include "pio_usb.h"
#include "pico/time.h"
#include "tusb.h"
#include "usb_definitions.h"
#include "host/usbh_pvt.h"

#include <algorithm>

extern "C" root_port_t pio_usb_root_port[];
extern "C" void pio_usb_host_irq_handler(std::uint8_t rootId);

namespace pixel_twins::rp2350 {
namespace {

constexpr std::uint8_t kNativeUsbRootHubPort = 0;
constexpr std::uint8_t kPioUsbRootHubPort = 1;
constexpr std::uint8_t kUsbPio = 2;
constexpr std::uint8_t kUsbDmaChannel = 15;
constexpr std::uint32_t kPioUsbDisconnectInterrupt = 1U << 1U;
UsbControllerInput* activeInput = nullptr;
repeating_timer_t pioUsbSofTimer{};
bool pioUsbSofTimerActive = false;

bool pioUsbSofTimerCallback(repeating_timer_t*) {
    pio_usb_host_frame();
    return true;
}

bool startPioUsbSofTimer() {
    if (pioUsbSofTimerActive) return true;
    pioUsbSofTimerActive = add_repeating_timer_us(
        -1000, pioUsbSofTimerCallback, nullptr, &pioUsbSofTimer);
    return pioUsbSofTimerActive;
}

void reenumeratePioUsbDevice() {
    auto* root = &pio_usb_root_port[0];
    if (!root->initialized || !root->connected) return;

    // Flash中のSOF欠落でdeviceが応答しなくなった場合は、物理的な
    // 抜き差しと同じ順序でremoveを通知した後、PIO自身の接続検出に
    // attachを発生させる。TinyUSBへattachを直接注入してはいけない。
    root->ints |= kPioUsbDisconnectInterrupt;
    pio_usb_host_irq_handler(0);
    root->connected = false;
    root->suspended = true;
    pio_usb_host_frame();
}

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
    // Flash中の遅延分をalarm poolが一気に追いつこうとしないよう、
    // SOFタイマーの開始時刻を保存処理後に張り直せる構成にする。
    config.skip_alarm_pool = true;

    activeInput = this;
    if (!tuh_configure(kPioUsbRootHubPort, TUH_CFGID_RPI_PIO_USB_CONFIGURATION,
                       &config)
        || !tuh_init(kNativeUsbRootHubPort)
        || !tuh_init(kPioUsbRootHubPort)
        || !startPioUsbSofTimer()) {
        activeInput = nullptr;
        return false;
    }
    initialized_ = true;
    return true;
}

void UsbControllerInput::task() noexcept {
    if (initialized_) tuh_task();
}

void UsbControllerInput::suspendPioHostForFlash() noexcept {
    if (!initialized_ || !pioUsbSofTimerActive) return;
    static_cast<void>(cancel_repeating_timer(&pioUsbSofTimer));
    pioUsbSofTimerActive = false;
}

void UsbControllerInput::resumePioHostAfterFlash() noexcept {
    if (!initialized_) return;
    reenumeratePioUsbDevice();
    hard_assert(startPioUsbSofTimer());
}

void UsbControllerInput::update(Controllers& controllers) noexcept {
    std::array<ControllerSample, kControllerCount> samples{};
    std::transform(slots_.begin(), slots_.end(), samples.begin(),
                   [](const Slot& slot) { return slot.sample; });
    controllers.update(samples);
}

void UsbControllerInput::mount(
    std::uint8_t rootHubPort, std::uint8_t deviceAddress,
    std::uint8_t instance,
    std::uint16_t vendorId, std::uint16_t productId) noexcept {
    const auto kind = identifySonyController(vendorId, productId);
    if (kind == SonyControllerKind::unsupported
        || rootHubPort >= slots_.size()) return;

    auto& slot = slots_[rootHubPort];
    // 複数HID interfaceを持つ機器では、最初に認識したinterfaceだけを使う。
    if (slot.deviceAddress != 0) return;
    slot = {};
    slot.deviceAddress = deviceAddress;
    slot.instance = instance;
    slot.kind = static_cast<std::uint8_t>(kind);
    slot.sample.connected = true;
    slot.sample.gamepad = true;
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
        usbh_get_rhport(devAddr), devAddr, instance, vendorId, productId);
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
