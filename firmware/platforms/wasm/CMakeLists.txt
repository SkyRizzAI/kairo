add_library(nema_platform_wasm STATIC
    src/wasm_platform.cpp
    src/wasm_cable_transport.cpp
    # Virtual WiFi router (pure C++, no stdio): the browser device gets a working
    # WiFi app + the Forge WiFi panel can inject networks. Moved here from the old
    # native sim platform, which was removed — the simulator is WASM now.
    src/sim_wifi_driver.cpp
    src/sim_wifi_radio.cpp    # raw radio HAL (Plan 87 Fase 4)
    src/wasm_wallet_rng.cpp   # trezor-crypto RNG hooks (Plan 94)
    src/wasm_speaker.cpp      # audio output → Web Audio bridge (Sounds test beep)
)

target_include_directories(nema_platform_wasm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(nema_platform_wasm PUBLIC nema_core)
