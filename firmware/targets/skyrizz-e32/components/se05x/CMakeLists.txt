# NXP Plug & Trust nano-package (SE050) — ESP-IDF component.
# Vendored lib (apdu + t1oi2c, the correct T=1'oI2C protocol) + thin ESP32 platform
# glue (port/: I2C over Arduino Wire, FreeRTOS timing). Auth = None (plain session) —
# enough for AES seed-sealing (Plan 96, mode B). SCP03 sources are NOT compiled.
# License: Apache-2.0 (see LICENSE.txt).
idf_component_register(
    SRCS
        "vendor/apdu/se05x_APDU_impl.c"
        "vendor/apdu/se05x_tlv.c"
        "vendor/apdu/smCom.c"
        "vendor/t1oi2c/phNxpEse_Api.c"
        "vendor/t1oi2c/phNxpEsePal_i2c.c"
        "vendor/t1oi2c/phNxpEseProto7816_3.c"
        "port/sm_i2c.cpp"
        "port/sm_timer.c"
    INCLUDE_DIRS
        "vendor/apdu"
        "vendor/apdu/scp03"
        "vendor/t1oi2c"
        "port"
    REQUIRES
        espressif__arduino-esp32   # i2cBusHandle() — share Wire's i2c_master bus (no conflict)
        esp_driver_i2c             # i2c_master_* + scl_wait_us (clock-stretch tolerant)
        esp_rom                    # esp_rom_delay_us
)

# T=1'oI2C transport selection (UM11225), as the nano-package CMake sets.
target_compile_definitions(${COMPONENT_LIB} PUBLIC T1oI2C T1oI2C_UM11225)

# Vendored third-party C — silence its warnings so they don't drown our build.
target_compile_options(${COMPONENT_LIB} PRIVATE -w)
