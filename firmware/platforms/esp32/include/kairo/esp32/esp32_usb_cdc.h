#pragma once
#include "kairo/hal/usb_cdc.h"

class HWCDC;   // arduino-esp32 USB-Serial-JTAG driver

namespace kairo {

// The USB-Serial-JTAG port. arduino-esp32 only defines its global `HWCDCSerial`
// (and only maps `Serial` to it) when ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
// are set — those are Arduino-IDE build flags that do not exist as IDF Kconfig
// symbols, so under arduino-as-component `Serial` is UART0 and HWCDCSerial is
// absent. Kairo owns the single HWCDC instance instead; boot banners and the KLP
// transport below must both go through this accessor.
HWCDC& usbSerialJtag();

// Esp32UsbCdc — IUsbCdc over the ESP32-S3 USB-Serial-JTAG (usbSerialJtag()). A
// raw byte pipe for KLP (Plan 34/35/37). A reader task polls the port and fans
// bytes to onData(). The KLP FrameParser tolerates interleaved console text via
// magic-byte resync, so this coexists with the log console on the same wire.
class Esp32UsbCdc : public IUsbCdc {
public:
    void start();   // begin the port + spawn the reader task

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
