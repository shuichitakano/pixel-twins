#pragma once

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

#define CFG_TUH_ENABLED 1
#ifndef CFG_TUH_RPI_PIO_USB
#define CFG_TUH_RPI_PIO_USB 0
#endif

// USB1とUSB2へコントローラーを各1台直結する。HUBは使用しない。
#define CFG_TUH_HUB 0
#define CFG_TUH_DEVICE_MAX 2
#define CFG_TUH_HID 2
#define CFG_TUH_CDC 0
#define CFG_TUH_MSC 0
#define CFG_TUH_VENDOR 0
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HID_EP_BUFSIZE 64

#define BOARD_TUH_RHPORT 1
#define BOARD_TUH_MAX_SPEED OPT_MODE_FULL_SPEED
