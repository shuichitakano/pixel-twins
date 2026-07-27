#pragma once

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

#define CFG_TUH_ENABLED 1
#define CFG_TUH_RPI_PIO_USB 1

// 1台のUSBハブを介して2台のコントローラーを接続できる構成。
#define CFG_TUH_HUB 1
#define CFG_TUH_DEVICE_MAX 3
#define CFG_TUH_HID 3
#define CFG_TUH_CDC 0
#define CFG_TUH_MSC 0
#define CFG_TUH_VENDOR 0
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HID_EP_BUFSIZE 64

#define BOARD_TUH_RHPORT 1
#define BOARD_TUH_MAX_SPEED OPT_MODE_FULL_SPEED
