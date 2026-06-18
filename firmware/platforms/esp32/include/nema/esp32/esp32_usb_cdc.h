#pragma once
#include "nema/hal/usb_cdc.h"

namespace nema {

class Esp32UsbCdc : public IUsbCdc {
public:
    void start();

    const char* name() const override { return "Esp32UsbCdc"; }
    bool   isOpen() const override;
    size_t write(const uint8_t* data, size_t len) override;
    void   onData(RecvFn fn, void* user) override { cb_ = fn; cbUser_ = user; }

    // Called from TinyUSB CDC RX callback (internal)
    void onRx();

    RecvFn cb_     = nullptr;
    void*  cbUser_ = nullptr;
};

} // namespace nema
