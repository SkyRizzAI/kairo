# SkyRizz E32 — ESP-IDF + arduino-esp32 project.
# ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB Octal PSRAM.
# Display: TFT LCD via FPC1 SPI. Buttons: XL9535 expander (3 nav buttons).
cmake_minimum_required(VERSION 3.20)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../core"               # → core
    "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp32"    # → esp32
    "${CMAKE_CURRENT_LIST_DIR}/../../boards/skyrizz-e32" # → skyrizz-e32
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/quickjs"     # → quickjs (Plan 37)
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/wasm3"       # → wasm3 (Plan 57)
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(palanu-skyrizz-e32)
