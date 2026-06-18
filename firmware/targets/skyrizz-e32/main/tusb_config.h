#pragma once

// TinyUSB configuration for SkyRizz E32 composite CDC + HID device.
// CFG_TUSB_MCU is injected by espressif__tinyusb's CMakeLists via -DCFG_TUSB_MCU=...

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS  OPT_OS_FREERTOS
#endif

// Explicit FreeRTOS include prefix so osal_freertos.h resolves FreeRTOS.h
// correctly regardless of whether CFG_TUSB_MCU is set at compile time.
#ifndef CFG_TUSB_OS_INC_PATH
#define CFG_TUSB_OS_INC_PATH  freertos/
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG  0
#endif

// Device mode enabled
#define CFG_TUD_ENABLED  1

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN  __attribute__((aligned(4)))
#endif

// Endpoint 0 size — arduino sets this from target in esp32-hal-tinyusb.h
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  64
#endif

// Device classes: CDC ACM (KLP serial) + HID keyboard (BadUSB)
#define CFG_TUD_CDC     1
#define CFG_TUD_HID     1
#define CFG_TUD_MSC     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

// CDC buffers
#define CFG_TUD_CDC_RX_BUFSIZE  512
#define CFG_TUD_CDC_TX_BUFSIZE  512
#define CFG_TUD_CDC_EP_BUFSIZE  64

// HID buffer
#define CFG_TUD_HID_EP_BUFSIZE  64
