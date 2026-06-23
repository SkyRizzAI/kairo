#include "nema/esp32/esp32_usb_cdc.h"
#include <Arduino.h>              // defines ARDUINO_USB_CDC_ON_BOOT (to 0 in JTAG mode)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <mutex>                  // serialize concurrent frame writes (Plan 88)

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
    // Plan 88 N1: the read buffer lives in .bss, NOT on the task stack. This task
    // now runs the full file-op chain inline (handleFile → FAT/LittleFS scan), whose
    // deep call tree must fit the task stack; a 1 KB on-stack buffer competing with
    // it was a stack-overflow setup. Single task instance → static is safe.
    static uint8_t buf[1024];
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
    // HWCDC's RX queue defaults to 256 bytes. The ISR drains the 64-byte hardware
    // FIFO into it byte-by-byte; if cdc_rx is briefly preempted (by the GUI task or
    // while the device streams logs/events), an inbound PLP frame larger than ~256 B
    // (e.g. a 1 KB file-write chunk) overflows the queue and bytes are dropped → the
    // frame fails CRC and is never processed. A 4 KB queue absorbs a full burst even
    // under preemption, so large host→device frames arrive intact (Plan 88).
    s_hwcdc.setRxBufferSize(4096);
    s_hwcdc.setTxBufferSize(4096);
    s_hwcdc.begin();
#endif
    // 12KB: cdc_rx runs the remote-service chain INLINE (handleFile → FATFS → SDSPI).
    // The stack lives in SCARCE internal RAM, shared with WiFi/BLE/I2S-DMA/etc. — a
    // 32 KB stack (to survive deep large-file FATFS flushes) starved the whole system
    // (WiFi `m f null`, mDNS/httpd alloc failures). 12 KB is the frugal middle: fine
    // for app-sized files; very large SD writes (>~64 KB) are an edge case on this
    // RAM-tight board — use /system (LittleFS) or smaller files (Plan 88).
    xTaskCreate(cdc_reader_task, "cdc_rx", 12288, nullptr, 5, nullptr);
}

bool Esp32UsbCdc::isOpen() const {
#if ARDUINO_USB_CDC_ON_BOOT
    return (bool)Serial;
#else
    return HWCDC::isConnected();
#endif
}

size_t Esp32UsbCdc::write(const uint8_t* data, size_t len) {
    // Serialize whole-frame writes here (transport level), NOT in LinkService: the GUI
    // screen-tap thread, the cdc_rx task (PONG/CLI/OTA acks) and app threads all send
    // concurrently, and HWCDC/Serial may write in chunks — interleaving two frames
    // corrupts the wire (lost OTA ack, random restarts; obs 7494). A LinkService-level
    // lock instead deadlocked the WASM simulator, so it lives in the device transport.
    static std::mutex s_writeMtx;
    std::lock_guard<std::mutex> lk(s_writeMtx);
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
