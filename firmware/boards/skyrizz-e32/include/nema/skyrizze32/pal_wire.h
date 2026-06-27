#pragma once
#include <cstdint>
#include <cstddef>

// PalWire — drop-in replacement for Arduino `Wire`, backed by the ESP-IDF **legacy** I2C
// driver (driver/i2c.h). The arduino-esp32 3.x `Wire` uses the new i2c_master driver,
// which is known-broken for **clock-stretching** slaves (ESP-IDF issues #14401/#14464/
// #11947/#14667) — and the SE050 clock-stretches during T=1'oI2C, which crashes it
// (assert in i2c_master_receive). The legacy driver tolerates clock-stretch with a long
// per-transaction timeout (proven by the StetelThings SE050-ESP32 reference). All board
// I2C (XL9535, touch, mic, camera, SE050) shares one bus, so it all moves to legacy
// (legacy + new can't coexist on one port). Plan 96.
//
// Board drivers keep their code: they include this header instead of <Wire.h>, and the
// `Wire` macro below redirects every `Wire.foo()` call to the shared legacy instance.

namespace nema::skyrizze32 {

class PalWire {
public:
    bool    begin(int sda, int scl);          // i2c_param_config + i2c_driver_install (legacy)
    void    setClock(uint32_t hz);

    void    beginTransmission(uint8_t addr);
    size_t  write(uint8_t b);
    size_t  write(const uint8_t* data, size_t n);
    uint8_t endTransmission(bool stop = true); // 0 = ACK/ok (Arduino convention)

    size_t  requestFrom(uint8_t addr, size_t n);
    size_t  requestFrom(int addr, int n) { return requestFrom((uint8_t)addr, (size_t)n); }
    int     available();
    int     read();

private:
    static constexpr size_t kBuf = 256;       // board reads are tiny; SE050 uses legacy directly
    uint8_t  txBuf_[kBuf]; size_t txLen_ = 0;  uint8_t txAddr_ = 0;
    uint8_t  pendBuf_[64]; size_t pendLen_ = 0; bool pendingWrite_ = false; // endTransmission(false)
    uint8_t  rxBuf_[kBuf]; size_t rxLen_ = 0, rxPos_ = 0;
    uint32_t freq_ = 400000;
    int      sda_ = -1, scl_ = -1;
    bool     installed_ = false;
};

extern PalWire palWire;

}  // namespace nema::skyrizze32

// Redirect Arduino `Wire` usage in board drivers to the legacy-backed shim.
#define Wire ::nema::skyrizze32::palWire
