# Palanu Dev Board — ESP-IDF + arduino-esp32 project.
# EXTRA_COMPONENT_DIRS lists PARENT dirs; ESP-IDF registers each child that has
# a CMakeLists.txt as a component. Component name = directory basename:
#   firmware/core            → component "core"
#   firmware/platforms/esp32 → component "esp32"
#   firmware/boards/dev-board→ component "dev-board"
#   firmware/vendor/arduino-libs/{GxEPD2,Adafruit_GFX,Adafruit_BusIO}
cmake_minimum_required(VERSION 3.20)

# A dir WITH a CMakeLists.txt is registered as a component directly; a dir
# WITHOUT one is scanned for child components. So point at each component dir
# directly (firmware/ itself has a host project() CMakeLists — must NOT include it).
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../core"                  # → core
    "${CMAKE_CURRENT_LIST_DIR}/../../aether"                # → aether (Plan 80)
    "${CMAKE_CURRENT_LIST_DIR}/../../servers/fbcon"      # → fbcon (text console, Plan 80)
    "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp32"       # → esp32
    "${CMAKE_CURRENT_LIST_DIR}/../../boards/dev-board"      # → dev-board
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/arduino-libs"   # → GxEPD2, Adafruit_* (container)
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/quickjs"        # → quickjs (Plan 37)
    "${CMAKE_CURRENT_LIST_DIR}/../../vendor/wasm3"          # → wasm3 (Plan 57)
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(palanu-dev-board)
