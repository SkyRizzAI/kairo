# Aether — the 1-bit canvas display server (Plan 80 / ADR 0002).
#
# A swappable display-server module: it OWNS all presentation (UI model, widgets,
# themes, fonts & text role mapping, renderer, component system, screens, the JS
# UI app runtime) and the GUI render loop (GuiService). It depends ONE-WAY on nema
# core (IDisplayDriver, Canvas, nema::display fonts, IDisplayServer, IScreen,
# ViewDispatcher). Core must never depend on this lib.
#
# The sources physically live under ../core/src (their include paths are still
# nema/...), but they are COMPILED INTO this lib, not nema_core — that is what
# makes the boundary real. nema_core's source list excludes every file here.
#
# Dual build like nema_core: ESP-IDF component on device, plain static lib on host.

set(AETHER_SRCS
    src/aether_info.cpp
    src/boot_display.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/hit_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/text_input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/virtual_keyboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/text_style.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/style_tokens.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/icon_pack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/single_foreground_policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/aether_server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/aether_abi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/focus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/component_runtime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/component_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/app_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/animation_player.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/animation_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/builtin_animations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/dolphin_showcase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/asset_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/asset_arena.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/status_bar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/components.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/ui/ascii_board_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/services/gui_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/app/component_app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/apps/hello_app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/apps/js_app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/apps/js_app_store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/apps/bad_usb_app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/apps/badusb_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/apps/dolphin_app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/js/js_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/js/js_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/js/js_runtime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/js/nema_host_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/home_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/app_list_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/file_browser_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/logs_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/settings_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/wifi_settings_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/wifi_network_detail_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/wifi_ip_config_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/bluetooth_settings_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/remote_settings_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/about_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/lock_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/close_and_open_modal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/sleep_settings_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/controls_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/touch_settings_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/sounds_settings_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/camera_settings_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/developer_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/profile_settings_screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/screens/dolphin_demo.cpp
)

if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${AETHER_SRCS}
        INCLUDE_DIRS "include"
        REQUIRES core quickjs wasm3_idf
    )
else()
    add_library(aether STATIC ${AETHER_SRCS})
    target_include_directories(aether PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(aether PUBLIC cxx_std_17)
    target_link_libraries(aether PUBLIC nema_core quickjs wasm3)
endif()
