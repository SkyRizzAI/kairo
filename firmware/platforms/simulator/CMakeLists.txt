add_library(kairo_platform_sim STATIC
    src/simulator_platform.cpp
    src/sim_battery_driver.cpp
    src/sim_wifi_driver.cpp
    src/sim_http_client.cpp
    src/sim_display.cpp
    src/mem_config_store.cpp
    src/json_stdout_sink.cpp
    src/telemetry_bridge.cpp
    src/command_reader.cpp
)

target_include_directories(kairo_platform_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kairo_platform_sim PUBLIC kairo_vendor)
target_link_libraries(kairo_platform_sim PRIVATE kairo_core)
