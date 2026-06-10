#include "kairo/esp32/esp32_usb_cdc.h"
#include <Arduino.h>
#include <HWCDC.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace kairo {

// Kairo-owned HWCDC instance — see the note in esp32_usb_cdc.h for why the
// arduino-esp32 global `HWCDCSerial` does not exist under arduino-as-component.
HWCDC& usbSerialJtag() {
    static HWCDC port;
    return port;
}

void Esp32UsbCdc::start() {
    auto& usb = usbSerialJtag();
    usb.setRxBufferSize(2048);   // host can burst (OTA chunks) between polls
    usb.begin();
    xTaskCreate(&Esp32UsbCdc::readerTask, "usbcdc_rx", 4096, this, 5, nullptr);
}

bool Esp32UsbCdc::isOpen() const {
    // USB is point-to-point: treat the pipe as always available so the device can
    // always reply (ACK / screen frames). HWCDC operator bool() is unreliable on
    // USB-Serial-JTAG (often false even when the host has the port open), which
    // would make the mux drop the handshake ACK → host stuck "connecting". The KLP
    // handshake itself establishes the real session.
    return true;
}

size_t Esp32UsbCdc::write(const uint8_t* data, size_t len) {
    return usbSerialJtag().write(data, len);
}

void Esp32UsbCdc::readerTask(void* arg) {
    auto* self = static_cast<Esp32UsbCdc*>(arg);
    auto& usb  = usbSerialJtag();
    uint8_t buf[256];
    for (;;) {
        int n = 0;
        while (n < (int)sizeof(buf) && usb.available() > 0)
            buf[n++] = (uint8_t)usb.read();
        if (n > 0 && self->cb_) self->cb_(self->cbUser_, buf, (size_t)n);
        else vTaskDelay(pdMS_TO_TICKS(5));
    }
}

} // namespace kairo
