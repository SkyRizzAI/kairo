#include "nema/esp32/esp32_usb_cdc.h"
#include <Arduino.h>              // defines ARDUINO_USB_CDC_ON_BOOT (to 0 in JTAG mode)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// IMPORTANT preprocessor note: arduino-esp32's HardwareSerial.h does
//   #ifndef ARDUINO_USB_CDC_ON_BOOT
//   #define ARDUINO_USB_CDC_ON_BOOT 0
// so after <Arduino.h> the macro is ALWAYS defined — to 1 in USB-CDC mode and to
// 0 in JTAG/Serial mode. We therefore test its VALUE with #if, never #ifdef/#ifndef
// (which would always be true and silently select the wrong branch).
//
// USB-CDC mode  (ARDUINO_USB_CDC_ON_BOOT=1): Serial == USBSerial (native USB CDC,
//   TinyUSB). The browser's Web Serial talks to that CDC port. Used with USB HID
//   for BadUSB (Plan 66).
// JTAG/Serial mode (ARDUINO_USB_CDC_ON_BOOT=0): Serial == HardwareSerial(0) == UART0
//   (GPIO43/44) which on this board is occupied by the XL9535/SPI and NOT exposed to
//   the host. The host instead sees the built-in USB Serial/JTAG (HWCDC, /dev/cu.usbmodem*).
//   So for the PLP remote we must drive HWCDC directly, NOT Serial. HWCDC owns its own
//   ISR (LL-based) independent of the IDF secondary console's ROM-putc TX path.
#if !ARDUINO_USB_CDC_ON_BOOT
#  include "HWCDC.h"
   static HWCDC s_hwcdc;
#endif

static nema::Esp32UsbCdc* s_cdc = nullptr;

#if ARDUINO_USB_CDC_ON_BOOT
static void cdc_reader_task(void*) {
    for (;;) {
        if (s_cdc && Serial.available() > 0) {
            s_cdc->onRx();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#else
static void cdc_reader_task(void*) {
    uint8_t buf[256];
    for (;;) {
        int n = (int)s_hwcdc.read(buf, sizeof(buf));
        if (s_cdc && n > 0 && s_cdc->cb_)
            s_cdc->cb_(s_cdc->cbUser_, buf, (size_t)n);
        if (n <= 0)
            vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#endif

namespace nema {

void Esp32UsbCdc::start() {
    s_cdc = this;
#if !ARDUINO_USB_CDC_ON_BOOT
    s_hwcdc.begin();
#endif
    xTaskCreate(cdc_reader_task, "cdc_rx", 4096, nullptr, 5, nullptr);
}

bool Esp32UsbCdc::isOpen() const {
#if ARDUINO_USB_CDC_ON_BOOT
    return (bool)Serial;
#else
    return HWCDC::isConnected();
#endif
}

size_t Esp32UsbCdc::write(const uint8_t* data, size_t len) {
#if ARDUINO_USB_CDC_ON_BOOT
    return Serial.write(data, len);
#else
    return s_hwcdc.write(data, len);
#endif
}

void Esp32UsbCdc::onRx() {
#if ARDUINO_USB_CDC_ON_BOOT
    uint8_t buf[256];
    size_t n = 0;
    while (n < sizeof(buf) && Serial.available() > 0) {
        buf[n++] = (uint8_t)Serial.read();
    }
    if (n > 0 && cb_) cb_(cbUser_, buf, n);
#endif
}

} // namespace nema
