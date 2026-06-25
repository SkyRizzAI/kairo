# Nema Core - hardware-agnostic C++ runtime.
# Dual-build: host (plain CMake, for simulator) and ESP-IDF (idf_component_register).

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/nema_version.cmake")

# Plan 48/49: IDL codegen paths (host build only - IDF uses pre-generated files)
set(NEMA_IDL_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../../")
set(NEMA_IDL_AST   "${NEMA_IDL_ROOT}/api/build/nema-api.json")
set(NEMA_GENERATED "${NEMA_IDL_ROOT}/generated")

set(NEMA_CORE_SRCS
    src/link/plp_codec.cpp
src/link/link_service.cpp
src/services/remote_service.cpp
src/services/remote_auth.cpp
src/hal/remote_screen_tap.cpp
src/input/gesture.cpp
    src/input/i_key_map.cpp
    src/input/i_touch_driver.cpp
    src/services/input_service.cpp
    src/runtime.cpp
    src/log/logger.cpp
    src/log/console_sink.cpp
    src/log/memory_sink.cpp
    src/log/file_sink.cpp
    src/event/event_bus.cpp
    src/event/async_event_poster.cpp
    src/nema/task_runner.cpp
    src/hal/async_display.cpp
    src/service/service_manager.cpp
    src/system/hardware_registry.cpp
    src/system/capability_registry.cpp
    src/system/board_profile.cpp
    src/fs/mem_filesystem.cpp
    src/fs/vfs.cpp
    src/fs/app_storage.cpp
    src/crypto/sha256.cpp
    src/wallet/hd_wallet.cpp
    src/wallet/nvs_backend.cpp
    src/wallet/se_backend.cpp
    src/wallet/soft_secure_element.cpp
    src/wallet/chains/rlp.cpp
    src/wallet/network_registry.cpp
    src/wallet/chains/evm.cpp
    src/wallet/chains/solana.cpp
    src/wallet/chains/bitcoin.cpp
    src/wallet/wallet_service.cpp
    src/wallet/wallet_consent_service.cpp
    src/wallet/wallet_controller.cpp
    src/wallet/wallet_vault.cpp
    src/wallet/wallet_system.cpp
    src/services/profile_service.cpp
    src/services/clock_service.cpp
    src/services/dummy_battery_driver.cpp
    src/services/ntp_service.cpp
    src/services/cli_service.cpp
    src/app/app_registry.cpp
    src/apps/wallets_app.cpp
    src/app/app_context.cpp
    src/app/app_host.cpp
    src/app/app_host_manager.cpp
    src/app/papp_package.cpp
    src/ui/canvas.cpp
    src/ui/font_registry.cpp
    src/ui/bmf_loader.cpp
    src/ui/font_5x8.cpp
    src/ui/font_6x8.cpp
    src/ui/font_reg8.cpp
    src/ui/font_bold8.cpp
    src/ui/font_reg10.cpp
    src/ui/font_bold10.cpp
    src/ui/font_reg12.cpp
    src/ui/font_bold12.cpp
    src/ui/font_iosk_reg8.cpp
    src/ui/font_iosk_bold8.cpp
    src/ui/font_iosk_reg10.cpp
    src/ui/font_iosk_bold10.cpp
    src/ui/font_iosk_reg12.cpp
    src/ui/font_iosk_bold12.cpp
    src/ui/view_dispatcher.cpp
    src/ui/virtual_list.cpp
    src/ui/lazy_dir_loader.cpp
    src/services/storage_service.cpp
    src/services/permission_service.cpp
    src/services/resource_broker.cpp
    src/services/system_wifi_manager.cpp
    src/services/display_power_manager.cpp
    src/services/audio_service.cpp
    src/services/camera_service.cpp
    src/proc/stream.cpp
    src/proc/process_host.cpp
    src/proc/process_manager.cpp
    src/proc/pipe.cpp
    src/wasm/wasm_engine.cpp
    src/wasm/wasm_wasi.cpp
    src/wasm/wasm_nema.cpp
    src/wasm/wasm_canvas.cpp
    src/wasm/wasm_ui.cpp
    src/wasm/wasm_input.cpp
    src/wasm/wasm_wifi.cpp
    src/wasm/wasm_audio.cpp
    src/wasm/wasm_runtime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../generated/host/nema_api_quickjs.gen.cpp
)

if(ESP_PLATFORM)
    # --- ESP-IDF component build ---
    list(APPEND NEMA_CORE_SRCS src/nema/thread_esp32.cpp)
    idf_component_register(
        SRCS ${NEMA_CORE_SRCS}
        INCLUDE_DIRS "include" "${CMAKE_BINARY_DIR}/nema_generated" "${CMAKE_CURRENT_SOURCE_DIR}/../../generated" "${CMAKE_CURRENT_SOURCE_DIR}/../vendor"
        REQUIRES quickjs wasm3_idf trezor-crypto
    )
else()
    # --- Host build (simulator) ---
    list(APPEND NEMA_CORE_SRCS src/nema/thread_host.cpp)
    add_library(nema_core STATIC ${NEMA_CORE_SRCS})
    target_include_directories(nema_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/nema_generated
        ${CMAKE_CURRENT_SOURCE_DIR}/../../generated)
    target_compile_features(nema_core PUBLIC cxx_std_17)
    find_package(Threads REQUIRED)
    target_link_libraries(nema_core PUBLIC Threads::Threads quickjs wasm3 nema_vendor trezor-crypto)

    # Plan 48/49: IDL codegen - only needed for the host build (pre-generated
    # files are committed and used by the IDF build directly).
    find_program(BUN_EXECUTABLE bun)
    if(BUN_EXECUTABLE)
        add_custom_target(nema_idl_codegen
            COMMAND ${BUN_EXECUTABLE} run "${NEMA_IDL_ROOT}/packages/idl/src/parser.ts"
                        --out "${NEMA_IDL_AST}"
            COMMAND ${BUN_EXECUTABLE} run "${NEMA_IDL_ROOT}/packages/idl/src/gen.ts"
                        --ast "${NEMA_IDL_AST}"
                        --out "${NEMA_GENERATED}"
            WORKING_DIRECTORY "${NEMA_IDL_ROOT}"
            COMMENT "Regenerating Nema SDK bindings from api/*.pidl"
            VERBATIM
        )
        add_dependencies(nema_core nema_idl_codegen)
    else()
        message(STATUS "bun not found - skipping IDL codegen (Plan 48/49); using pre-generated files")
    endif()
endif()
