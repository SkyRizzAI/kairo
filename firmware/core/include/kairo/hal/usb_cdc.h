#pragma once
#include "kairo/hal/driver.h"
#include <cstdint>
#include <cstddef>

// Connectivity foundation — USB (Plan 34, Layer 1).
//
// Native USB as a composite device (TinyUSB on ESP32-S3). This is the CDC-ACM
// data class: a raw byte pipe wrapped by the remote layer's UsbCdcTransport
// (Plan 35). MSC (mount microSD to a PC) and HID are future classes on the same
// composite device. USB is point-to-point and physically secure, so there is no
// pairing here — the KLP handshake (Plan 35) still applies on top.
namespace kairo {

struct IUsbCdc : IDriver {
    DriverKind kind() const override { return DriverKind::Other; }

    virtual bool   isOpen() const = 0;                       // host opened the port?
    virtual size_t write(const uint8_t* data, size_t len) = 0;  // to host
    using RecvFn = void (*)(void* user, const uint8_t* data, size_t len);
    virtual void   onData(RecvFn fn, void* user) = 0;          // from host
};

} // namespace kairo
