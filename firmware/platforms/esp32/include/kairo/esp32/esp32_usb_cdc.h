#pragma once
#include "kairo/hal/usb_cdc.h"

namespace kairo {

// Esp32UsbCdc — IUsbCdc over the ESP32-S3 USB-CDC that Arduino exposes as the
// global `Serial` (HWCDC = USB-Serial-JTAG, or TinyUSB CDC depending on
// ARDUINO_USB_MODE). A raw byte pipe for KLP (Plan 34/35/37). A reader task polls
// Serial and fans bytes to onData(). The KLP FrameParser tolerates interleaved
// console text via magic-byte resync, so this coexists with the log console.
class Esp32UsbCdc : public IUsbCdc {
public:
    void start();   // spawn the reader task (Serial is already begun by Arduino)

    const char* name() const override { return "Esp32UsbCdc"; }
    bool   isOpen() const override;
    size_t write(const uint8_t* data, size_t len) override;
    void   onData(RecvFn fn, void* user) override { cb_ = fn; cbUser_ = user; }

private:
    static void readerTask(void* arg);
    RecvFn cb_     = nullptr;
    void*  cbUser_ = nullptr;
};

} // namespace kairo
