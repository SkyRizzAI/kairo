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
src/hal/remote_screen_tap.cpp
src/input/gesture.cpp
    src/input/i_key_map.cpp
    src/input/i_touch_driver.cpp
    src/ui/hit_test.cpp
    src/services/input_service.cpp
    src/runtime.cpp
    src/log/logger.cpp
    src/log/console_sink.cpp
    src/log/memory_sink.cpp
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
    src/crypto/sha256.cpp
    src/services/profile_service.cpp
    src/services/clock_service.cpp
    src/services/dummy_battery_driver.cpp
    src/services/ntp_service.cpp
    src/services/cli_service.cpp
    src/services/gui_service.cpp
    src/app/app_registry.cpp
    src/app/app_host.cpp
    src/app/app_host_manager.cpp
    src/app/component_app.cpp
    src/app/papp_package.cpp
    src/app/papp_installer.cpp
    src/apps/hello_app.cpp
    src/apps/js_app.cpp
    src/apps/js_app_store.cpp
    src/apps/bad_usb_app.cpp
    src/apps/badusb_parser.cpp
    src/ui/text_input.cpp
    src/ui/virtual_keyboard.cpp
    src/ui/canvas.cpp
    src/ui/layout.cpp
    src/ui/draw.cpp
    src/ui/widgets.cpp
    src/ui/text_style.cpp
    src/ui/style_tokens.cpp
    src/ui/renderer.cpp
    src/ui/icon_pack.cpp
    src/ui/single_foreground_policy.cpp
    src/ui/aether_server.cpp
    src/ui/aether_abi.cpp
    src/ui/fbcon_server.cpp
    src/ui/focus.cpp
    src/ui/component_runtime.cpp
    src/ui/component_screen.cpp
    src/ui/font_5x8.cpp
    src/ui/view_dispatcher.cpp
    src/ui/status_bar.cpp
    src/ui/components.cpp
    src/ui/ascii_board_renderer.cpp
    src/screens/home_screen.cpp
    src/screens/app_list_screen.cpp
    src/screens/logs_screen.cpp
    src/screens/settings_screen.cpp
    src/screens/about_screen.cpp
    src/screens/lock_screen.cpp
    src/screens/close_and_open_modal.cpp
    src/screens/sleep_settings_screen.cpp
    src/screens/controls_screen.cpp
    src/screens/touch_settings_screen.cpp
    src/screens/sounds_settings_screen.cpp
    src/screens/camera_settings_screen.cpp
    src/screens/developer_screen.cpp
    src/screens/profile_settings_screen.cpp
    src/services/display_power_manager.cpp
    src/services/audio_service.cpp
    src/services/camera_service.cpp
    src/js/js_engine.cpp
    src/js/js_api.cpp
    src/js/js_runtime.cpp
    src/js/nema_host_impl.cpp
    src/proc/stream.cpp
    src/proc/process_host.cpp
    src/proc/process_manager.cpp
    src/proc/pipe.cpp
    src/wasm/wasm_engine.cpp
    src/wasm/wasm_wasi.cpp
    src/wasm/wasm_runtime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../generated/host/nema_api_quickjs.gen.cpp
)

if(ESP_PLATFORM)
    # --- ESP-IDF component build ---
    list(APPEND NEMA_CORE_SRCS src/nema/thread_esp32.cpp)
    idf_component_register(
        SRCS ${NEMA_CORE_SRCS}
        INCLUDE_DIRS "include" "${CMAKE_BINARY_DIR}/nema_generated" "${CMAKE_CURRENT_SOURCE_DIR}/../../generated" "${CMAKE_CURRENT_SOURCE_DIR}/../vendor"
        REQUIRES quickjs wasm3
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
    target_link_libraries(nema_core PUBLIC Threads::Threads quickjs wasm3 nema_vendor)

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
