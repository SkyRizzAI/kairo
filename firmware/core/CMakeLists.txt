# Kairo Core — hardware-agnostic C++ runtime.
# Dual-build: host (plain CMake, for simulator) and ESP-IDF (idf_component_register).

set(KAIRO_CORE_SRCS
    src/link/klp_codec.cpp
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
    src/services/clock_service.cpp
    src/services/cli_service.cpp
    src/services/gui_service.cpp
    src/app/app_registry.cpp
    src/app/app_host.cpp
    src/app/app_host_manager.cpp
    src/app/component_app.cpp
    src/apps/counter_app.cpp
    src/apps/clock_app.cpp
    src/apps/stopwatch_app.cpp
    src/apps/task_demo_app.cpp
    src/apps/wifi_app.cpp
    src/apps/bluetooth_app.cpp
    src/apps/ticker_app.cpp
    src/apps/touch_test_app.cpp
    src/apps/camera_app.cpp
    src/apps/ui_showcase_app.cpp
    src/apps/js_app.cpp
    src/apps/js_app_store.cpp
    src/ui/text_input.cpp
    src/ui/virtual_keyboard.cpp
    src/ui/canvas.cpp
    src/ui/layout.cpp
    src/ui/widgets.cpp
    src/ui/text_style.cpp
    src/ui/renderer.cpp
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
    src/services/display_power_manager.cpp
    src/services/audio_service.cpp
    src/services/camera_service.cpp
    src/js/js_engine.cpp
    src/js/js_api.cpp
)

if(ESP_PLATFORM)
    # --- ESP-IDF component build ---
    list(APPEND KAIRO_CORE_SRCS src/nema/thread_esp32.cpp)
    idf_component_register(
        SRCS ${KAIRO_CORE_SRCS}
        INCLUDE_DIRS "include"
        REQUIRES quickjs
    )
else()
    # --- Host build (simulator) ---
    list(APPEND KAIRO_CORE_SRCS src/nema/thread_host.cpp)
    add_library(kairo_core STATIC ${KAIRO_CORE_SRCS})
    target_include_directories(kairo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(kairo_core PUBLIC cxx_std_17)
    find_package(Threads REQUIRED)
    target_link_libraries(kairo_core PUBLIC Threads::Threads quickjs)
endif()
