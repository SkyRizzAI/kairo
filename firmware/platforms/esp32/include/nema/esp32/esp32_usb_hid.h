#pragma once
#include "nema/hal/usb_hid.h"
#include "nema/service.h"

namespace nema {

class Runtime;

class Esp32UsbHid : public IUsbHid, public IService {
public:
    void onRegister(Runtime& rt) override;

    const char* name() const override { return "Esp32UsbHid"; }
    void start() override;
    void stop() override;

    void sendKey(uint8_t modifier, uint8_t keycode) override;
    void sendString(const char* s, uint32_t delayMs = 0) override;
    void delay(uint32_t ms) override;
    void releaseAll() override;
    bool isReady() const override { return ready_; }

private:
    bool ready_ = false;
};

} // namespace nema
