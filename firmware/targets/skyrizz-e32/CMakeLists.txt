# SkyRizz E32 — ESP-IDF + arduino-esp32 project.
# ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB Octal PSRAM.
# Display: TFT LCD via FPC1 SPI. Buttons: XL9535 expander (3 nav buttons).
cmake_minimum_required(VERSION 3.20)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../core"               # → core
    "${CMAKE_CURRENT_LIST_DIR}/../../aether"             # → aether (display server, Plan 80)
    "${CMAKE_CURRENT_LIST_DIR}/../../servers/fbcon"      # → fbcon (text console, Plan 80)
    "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp32"    # → esp32
    "${CMAKE_CURRENT_LIST_DIR}/../../boards/skyrizz-e32" # → skyrizz-e32
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/quickjs"     # → quickjs (Plan 37)
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/wasm3_idf"   # → wasm3_idf — IDF wrapper (Plan 57)
)

# ---------------------------------------------------------------------------
# Enable raw TX of deauth/disassoc management frames.
# The closed-source WiFi blob's ieee80211_raw_frame_sanity_check() rejects
# frame subtypes 0xC0 (deauth) / 0xA0 (disassoc) inside esp_wifi_80211_tx(),
# producing "unsupport frame type: 0c0" — promiscuous mode does NOT bypass it.
# We WEAKEN that symbol in the prebuilt archive (objcopy) so our strong override
# in platforms/esp32/src/esp32_wifi_raw_override.c wins at link time. No
# instruction bytes are patched (byte-patching leaves dangling Xtensa
# relocations the linker cannot decode). Idempotent: skips if already weak.
# Runs at configure time, before the library path is resolved.
# ---------------------------------------------------------------------------
set(_wifi_lib "$ENV{IDF_PATH}/components/esp_wifi/lib/esp32s3/libnet80211.a")
find_program(_xt_ar NAMES xtensa-esp32s3-elf-ar
    PATHS "$ENV{HOME}/.espressif/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin" NO_DEFAULT_PATH)
find_program(_xt_objcopy NAMES xtensa-esp32s3-elf-objcopy
    PATHS "$ENV{HOME}/.espressif/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin" NO_DEFAULT_PATH)
if(_xt_ar AND _xt_objcopy AND EXISTS "${_wifi_lib}")
    execute_process(
        COMMAND "${CMAKE_CURRENT_LIST_DIR}/tools/patch_wifi_lib.py"
            "${_wifi_lib}" "${_xt_objcopy}" "${_xt_ar}" "${_wifi_lib}"
        RESULT_VARIABLE _patch_rc OUTPUT_VARIABLE _patch_out ERROR_VARIABLE _patch_err)
    if(_patch_rc EQUAL 0)
        message(STATUS "deauth patch: ${_patch_out}")
    else()
        message(WARNING "WiFi lib patch failed (deauth will not work): ${_patch_err}")
    endif()
else()
    message(WARNING "WiFi lib patch skipped — toolchain or libnet80211.a not found; "
                    "deauth/disassoc TX will fail with 'unsupport frame type'")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(palanu-skyrizz-e32)

# Inject our tusb_config.h globally so espressif__tinyusb can find it.
# arduino-esp32's esp32-hal-tinyusb.h includes tusb_config.h by name;
# adding main/ to the global include path satisfies that search.
idf_build_set_property(COMPILE_OPTIONS
    "-I${CMAKE_CURRENT_LIST_DIR}/main"
    APPEND)
idf_build_set_property(COMPILE_OPTIONS
    "-I${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__tinyusb/src"
    APPEND)
# CFG_TUSB_MCU activates the Espressif-specific spinlock path in osal_freertos.h.
# espressif__tinyusb sets this PUBLIC on its own target, but espressif__arduino-esp32
# does not REQUIRE that component, so esp32-hal-tinyusb.c never sees the flag.
# Injecting it globally ensures the correct vPortEnterCritical(mux) path is used.
idf_build_set_property(COMPILE_OPTIONS
    "-DCFG_TUSB_MCU=OPT_MCU_ESP32S3"
    APPEND)
# Plan 66 — CDC+HID composite. Preloader in esp32_usb_hid.cpp pre-loads the HID
# descriptor in main_task context (8KB stack) before usb_device_task (4KB) starts,
# preventing stack overflow on GET_HID_REPORT_DESCRIPTOR. PSRAM ensures malloc succeeds.
# Uncomment the two lines below to re-enable USB HID/CDC mode.
# idf_build_set_property(COMPILE_OPTIONS "-DARDUINO_USB_CDC_ON_BOOT=1" APPEND)
# idf_build_set_property(COMPILE_OPTIONS "-DARDUINO_USB_ON_BOOT=1"     APPEND)
