#include "nema/skyrizze32/pal_wire.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

namespace nema::skyrizze32 {

namespace {
constexpr i2c_port_t kPort   = I2C_NUM_0;
constexpr TickType_t kTimeout = pdMS_TO_TICKS(1000);   // tolerate SE050 clock-stretch
}

PalWire palWire;

bool PalWire::begin(int sda, int scl) {
    if (installed_) return true;
    sda_ = sda; scl_ = scl;
    i2c_config_t conf = {};
    conf.mode          = I2C_MODE_MASTER;
    conf.sda_io_num    = sda;
    conf.scl_io_num    = scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;    // match Arduino Wire (board has the bus pulled up)
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = freq_;
    if (i2c_param_config(kPort, &conf) != ESP_OK) return false;
    if (i2c_driver_install(kPort, I2C_MODE_MASTER, 0, 0, 0) != ESP_OK) return false;
    installed_ = true;
    return true;
}

void PalWire::setClock(uint32_t hz) {
    freq_ = hz;
    if (installed_ && sda_ >= 0) {              // re-apply clock on the live bus
        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = sda_; conf.scl_io_num = scl_;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE; conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = hz;
        i2c_param_config(kPort, &conf);
    }
}

void PalWire::beginTransmission(uint8_t addr) { txAddr_ = addr; txLen_ = 0; }

size_t PalWire::write(uint8_t b) {
    if (txLen_ < kBuf) { txBuf_[txLen_++] = b; return 1; }
    return 0;
}

size_t PalWire::write(const uint8_t* data, size_t n) {
    size_t w = 0;
    for (; w < n && txLen_ < kBuf; w++) txBuf_[txLen_++] = data[w];
    return w;
}

uint8_t PalWire::endTransmission(bool stop) {
    if (!installed_) return 4;
    if (!stop) {                                 // defer for a repeated-start read
        pendLen_ = txLen_ <= sizeof(pendBuf_) ? txLen_ : sizeof(pendBuf_);
        std::memcpy(pendBuf_, txBuf_, pendLen_);
        pendingWrite_ = true;
        return 0;
    }
    esp_err_t e = i2c_master_write_to_device(kPort, txAddr_, txBuf_, txLen_, kTimeout);
    return e == ESP_OK ? 0 : (e == ESP_ERR_TIMEOUT ? 5 : 2);   // 2 = NACK, 5 = timeout (Arduino)
}

size_t PalWire::requestFrom(uint8_t addr, size_t n) {
    rxLen_ = 0; rxPos_ = 0;
    if (!installed_ || n == 0 || n > kBuf) return 0;
    esp_err_t e;
    if (pendingWrite_) {                          // repeated-start write→read
        e = i2c_master_write_read_device(kPort, addr, pendBuf_, pendLen_, rxBuf_, n, kTimeout);
        pendingWrite_ = false;
    } else {
        e = i2c_master_read_from_device(kPort, addr, rxBuf_, n, kTimeout);
    }
    if (e != ESP_OK) return 0;
    rxLen_ = n;
    return n;
}

int PalWire::available() { return (int)(rxLen_ - rxPos_); }

int PalWire::read() { return rxPos_ < rxLen_ ? rxBuf_[rxPos_++] : -1; }

}  // namespace nema::skyrizze32
