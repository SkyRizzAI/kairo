#include "nema/esp32/esp32_usb_cdc.h"
#include <Arduino.h>
#include "USB.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static nema::Esp32UsbCdc* s_cdc = nullptr;

static void cdc_reader_task(void*) {
    for (;;) {
        if (s_cdc && Serial.available() > 0) {
            s_cdc->onRx();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

namespace nema {

void Esp32UsbCdc::start() {
    s_cdc = this;
    // Serial = USBSerial (USBCDC) and USB.begin() are both handled by arduino
    // startup before setup() runs (ARDUINO_USB_CDC_ON_BOOT + ARDUINO_USB_ON_BOOT).
    // Calling them again here would double-init TinyUSB and panic.
    xTaskCreate(cdc_reader_task, "cdc_rx", 4096, nullptr, 5, nullptr);
}

bool Esp32UsbCdc::isOpen() const {
    return (bool)Serial;
}

size_t Esp32UsbCdc::write(const uint8_t* data, size_t len) {
    return Serial.write(data, len);
}

void Esp32UsbCdc::onRx() {
    uint8_t buf[256];
    size_t n = 0;
    while (n < sizeof(buf) && Serial.available() > 0) {
        buf[n++] = (uint8_t)Serial.read();
    }
    if (n > 0 && cb_) cb_(cbUser_, buf, n);
}

} // namespace nema
