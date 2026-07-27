#include "host/hcd.h"
#include "host/usbh.h"

bool native_hcd_init(uint8_t, const tusb_rhport_init_t*);
bool native_hcd_deinit(uint8_t);
void native_hcd_port_reset(uint8_t);
void native_hcd_port_reset_end(uint8_t);
bool native_hcd_port_connect_status(uint8_t);
tusb_speed_t native_hcd_port_speed_get(uint8_t);
void native_hcd_device_close(uint8_t, uint8_t);
uint32_t native_hcd_frame_number(uint8_t);
void native_hcd_int_enable(uint8_t);
void native_hcd_int_disable(uint8_t);
bool native_hcd_edpt_open(uint8_t, uint8_t, const tusb_desc_endpoint_t*);
bool native_hcd_edpt_xfer(uint8_t, uint8_t, uint8_t, uint8_t*, uint16_t);
bool native_hcd_edpt_abort_xfer(uint8_t, uint8_t, uint8_t);
bool native_hcd_setup_send(uint8_t, uint8_t, const uint8_t[8]);
bool native_hcd_edpt_clear_stall(uint8_t, uint8_t, uint8_t);

bool pio_hcd_configure(uint8_t, uint32_t, const void*);
bool pio_hcd_init(uint8_t, const tusb_rhport_init_t*);
void pio_hcd_port_reset(uint8_t);
void pio_hcd_port_reset_end(uint8_t);
bool pio_hcd_port_connect_status(uint8_t);
tusb_speed_t pio_hcd_port_speed_get(uint8_t);
void pio_hcd_device_close(uint8_t, uint8_t);
uint32_t pio_hcd_frame_number(uint8_t);
void pio_hcd_int_enable(uint8_t);
void pio_hcd_int_disable(uint8_t);
bool pio_hcd_edpt_open(uint8_t, uint8_t, const tusb_desc_endpoint_t*);
bool pio_hcd_edpt_xfer(uint8_t, uint8_t, uint8_t, uint8_t*, uint16_t);
bool pio_hcd_edpt_abort_xfer(uint8_t, uint8_t, uint8_t);
bool pio_hcd_setup_send(uint8_t, uint8_t, const uint8_t[8]);
bool pio_hcd_edpt_clear_stall(uint8_t, uint8_t, uint8_t);

static bool is_native(uint8_t rhport) {
    return rhport == 0;
}

bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
    return is_native(rhport) ? true : pio_hcd_configure(rhport, cfg_id, cfg_param);
}

bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
    return is_native(rhport)
        ? native_hcd_init(rhport, rh_init)
        : pio_hcd_init(rhport, rh_init);
}

bool hcd_deinit(uint8_t rhport) {
    return is_native(rhport) ? native_hcd_deinit(rhport) : true;
}

void hcd_port_reset(uint8_t rhport) {
    if (is_native(rhport)) native_hcd_port_reset(rhport);
    else pio_hcd_port_reset(rhport);
}

void hcd_port_reset_end(uint8_t rhport) {
    if (is_native(rhport)) native_hcd_port_reset_end(rhport);
    else pio_hcd_port_reset_end(rhport);
}

bool hcd_port_connect_status(uint8_t rhport) {
    return is_native(rhport)
        ? native_hcd_port_connect_status(rhport)
        : pio_hcd_port_connect_status(rhport);
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
    return is_native(rhport)
        ? native_hcd_port_speed_get(rhport)
        : pio_hcd_port_speed_get(rhport);
}

void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
    if (is_native(rhport)) native_hcd_device_close(rhport, dev_addr);
    else pio_hcd_device_close(rhport, dev_addr);
}

uint32_t hcd_frame_number(uint8_t rhport) {
    return is_native(rhport)
        ? native_hcd_frame_number(rhport)
        : pio_hcd_frame_number(rhport);
}

void hcd_int_enable(uint8_t rhport) {
    if (is_native(rhport)) native_hcd_int_enable(rhport);
    else pio_hcd_int_enable(rhport);
}

void hcd_int_disable(uint8_t rhport) {
    if (is_native(rhport)) native_hcd_int_disable(rhport);
    else pio_hcd_int_disable(rhport);
}

bool hcd_edpt_open(
    uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t* ep) {
    return is_native(rhport)
        ? native_hcd_edpt_open(rhport, dev_addr, ep)
        : pio_hcd_edpt_open(rhport, dev_addr, ep);
}

bool hcd_edpt_xfer(
    uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr,
    uint8_t* buffer, uint16_t length) {
    return is_native(rhport)
        ? native_hcd_edpt_xfer(rhport, dev_addr, ep_addr, buffer, length)
        : pio_hcd_edpt_xfer(rhport, dev_addr, ep_addr, buffer, length);
}

bool hcd_edpt_abort_xfer(
    uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
    return is_native(rhport)
        ? native_hcd_edpt_abort_xfer(rhport, dev_addr, ep_addr)
        : pio_hcd_edpt_abort_xfer(rhport, dev_addr, ep_addr);
}

bool hcd_setup_send(
    uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]) {
    return is_native(rhport)
        ? native_hcd_setup_send(rhport, dev_addr, setup_packet)
        : pio_hcd_setup_send(rhport, dev_addr, setup_packet);
}

bool hcd_edpt_clear_stall(
    uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
    return is_native(rhport)
        ? native_hcd_edpt_clear_stall(rhport, dev_addr, ep_addr)
        : pio_hcd_edpt_clear_stall(rhport, dev_addr, ep_addr);
}
