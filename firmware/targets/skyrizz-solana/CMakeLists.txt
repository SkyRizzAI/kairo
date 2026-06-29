# SkyRizz Solana ("Lanyard v2") — ESP-IDF + arduino-esp32 project.
# ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB Octal PSRAM.
# Display: ILI9341 240×320 TFT via direct SPI. Buttons: TCA9534 6-button D-pad.
cmake_minimum_required(VERSION 3.20)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../core"                  # → core
    "${CMAKE_CURRENT_LIST_DIR}/../../aether"                # → aether (display server, Plan 80)
    "${CMAKE_CURRENT_LIST_DIR}/../../servers/fbcon"         # → fbcon (text console, Plan 80)
    "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp32"       # → esp32
    "${CMAKE_CURRENT_LIST_DIR}/../../boards/skyrizz-solana" # → skyrizz-solana
    "${CMAKE_CURRENT_LIST_DIR}/../skyrizz-e32/components/se05x" # → se05x (shared NXP nano-pkg, Plan 96)
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/quickjs"        # → quickjs (Plan 37)
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/wasm3_idf"      # → wasm3_idf — IDF wrapper (Plan 57)
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/trezor-crypto"  # → trezor-crypto — wallet crypto (Plan 94)
)

# ---------------------------------------------------------------------------
# Enable raw TX of deauth/disassoc management frames (see skyrizz-e32 + ADR 0011).
# Weaken libnet80211.a's ieee80211_raw_frame_sanity_check() so our strong override
# in main/wifi_raw_override.c wins at link time. Idempotent. Configure-time only.
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
project(palanu-skyrizz-solana)

# Inject our tusb_config.h globally so espressif__tinyusb can find it (see skyrizz-e32).
idf_build_set_property(COMPILE_OPTIONS
    "-I${CMAKE_CURRENT_LIST_DIR}/main"
    APPEND)
idf_build_set_property(COMPILE_OPTIONS
    "-I${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__tinyusb/src"
    APPEND)
idf_build_set_property(COMPILE_OPTIONS
    "-DCFG_TUSB_MCU=OPT_MCU_ESP32S3"
    APPEND)
# Uncomment to re-enable USB HID/CDC mode (Plan 66). NOTE: on this board GPIO19/20
# double as the PDM-mic clock/data, so native USB and the mics are mutually exclusive.
# idf_build_set_property(COMPILE_OPTIONS "-DARDUINO_USB_CDC_ON_BOOT=1" APPEND)
# idf_build_set_property(COMPILE_OPTIONS "-DARDUINO_USB_ON_BOOT=1"     APPEND)
