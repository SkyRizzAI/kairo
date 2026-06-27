/** @file sm_i2c.cpp
 *  @brief ESP32 I2C transport for the NXP nano-package — new i2c_master driver, SE050 device
 *         configured with scl_wait_us so the SE050's T=1'oI2C clock-stretch is tolerated.
 *
 * We do NOT use Arduino Wire's read path (it adds devices with scl_wait_us=0, so the SE050's
 * clock-stretch crashes i2c_master_receive). Instead we grab Wire's i2c_master BUS handle
 * (arduino-esp32 `i2cBusHandle`) and add the SE050 as our OWN device with a long scl_wait_us.
 * Same driver as the board (no driver_ng/legacy conflict); the board's devices stay on Wire.
 * Plan 96 (Opsi A2).
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "driver/i2c_master.h"

extern "C" {
#include "sm_i2c.h"

// arduino-esp32 hal accessor — returns the i2c_master_bus_handle_t for a bus (Wire = num 0).
extern void *i2cBusHandle(uint8_t i2c_num);

static i2c_master_dev_handle_t s_se = NULL;

i2c_error_t axI2CInit(void **conn_ctx, const char *pDevName)
{
    (void)pDevName;
    if (conn_ctx) *conn_ctx = NULL;
    if (s_se) return I2C_OK;

    i2c_master_bus_handle_t bus = (i2c_master_bus_handle_t)i2cBusHandle(0);   // Wire's bus
    if (!bus) return I2C_FAILED;

    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address  = 0x48;          // SE050 (SMCOM_I2C_ADDRESS 0x90 >> 1)
    cfg.scl_speed_hz    = 400000;
    cfg.scl_wait_us     = 1000000;       // tolerate SE050 clock-stretch (driver caps to HW max)
    if (i2c_master_bus_add_device(bus, &cfg, &s_se) != ESP_OK) { s_se = NULL; return I2C_FAILED; }
    return I2C_OK;
}

void axI2CTerm(void *conn_ctx, int mode)
{
    (void)conn_ctx; (void)mode;
    if (s_se) { i2c_master_bus_rm_device(s_se); s_se = NULL; }
}

i2c_error_t axI2CWrite(void *conn_ctx, unsigned char bus, unsigned char addr,
                       unsigned char *pTx, unsigned short txLen)
{
    (void)conn_ctx; (void)bus; (void)addr;          // address fixed in the device config
    if (!s_se) return I2C_FAILED;
    esp_err_t e = i2c_master_transmit(s_se, pTx, txLen, 1000);
    return (e == ESP_OK) ? I2C_OK : I2C_FAILED;
}

i2c_error_t axI2CRead(void *conn_ctx, unsigned char bus, unsigned char addr,
                      unsigned char *pRx, unsigned short rxLen)
{
    (void)conn_ctx; (void)bus; (void)addr;
    if (rxLen == 0) return I2C_OK;
    if (!s_se) return I2C_FAILED;
    esp_err_t e = i2c_master_receive(s_se, pRx, rxLen, 1000);
    return (e == ESP_OK) ? I2C_OK : I2C_FAILED;       // NACK/busy → caller retries
}

}  // extern "C"
