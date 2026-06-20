# Host unit tests for hardware-agnostic core logic.
add_executable(layout_test layout_test.cpp)
target_link_libraries(layout_test PRIVATE nema_core aether)

add_executable(plp_test plp_test.cpp)
target_link_libraries(plp_test PRIVATE nema_core aether)

add_executable(link_test link_test.cpp)
target_link_libraries(link_test PRIVATE nema_core aether)

add_executable(js_test js_test.cpp)
target_link_libraries(js_test PRIVATE nema_core aether)

add_executable(js_render_test js_render_test.cpp)
target_link_libraries(js_render_test PRIVATE nema_core aether)

enable_testing()
add_test(NAME layout_test COMMAND layout_test)
add_test(NAME plp_test COMMAND plp_test)
add_test(NAME link_test COMMAND link_test)
add_test(NAME js_test COMMAND js_test)
add_test(NAME js_render_test COMMAND js_render_test)

add_executable(service_test service_test.cpp)
target_link_libraries(service_test PRIVATE nema_core aether)
add_test(NAME service_test COMMAND service_test)

add_executable(js_dualimport_test js_dualimport_test.cpp)
target_link_libraries(js_dualimport_test PRIVATE nema_core aether)
add_test(NAME js_dualimport_test COMMAND js_dualimport_test)

add_executable(js_graceful_test js_graceful_test.cpp)
target_link_libraries(js_graceful_test PRIVATE nema_core aether)
add_test(NAME js_graceful_test COMMAND js_graceful_test)

add_executable(capability_test capability_test.cpp)
target_link_libraries(capability_test PRIVATE nema_core aether)
add_test(NAME capability_test COMMAND capability_test)

add_executable(cli_test cli_test.cpp)
target_link_libraries(cli_test PRIVATE nema_core aether)
add_test(NAME cli_test COMMAND cli_test)

add_executable(asset_test asset_test.cpp)
target_link_libraries(asset_test PRIVATE nema_core aether)
add_test(NAME asset_test COMMAND asset_test)

add_executable(wifi_contract_test wifi_contract_test.cpp)
target_link_libraries(wifi_contract_test PRIVATE nema_core aether)
add_test(NAME wifi_contract_test COMMAND wifi_contract_test)
