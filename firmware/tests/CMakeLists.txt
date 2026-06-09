# Host unit tests for hardware-agnostic core logic.
add_executable(layout_test layout_test.cpp)
target_link_libraries(layout_test PRIVATE kairo_core)

add_executable(klp_test klp_test.cpp)
target_link_libraries(klp_test PRIVATE kairo_core)

add_executable(link_test link_test.cpp)
target_link_libraries(link_test PRIVATE kairo_core)

add_executable(js_test js_test.cpp)
target_link_libraries(js_test PRIVATE kairo_core)

add_executable(js_render_test js_render_test.cpp)
target_link_libraries(js_render_test PRIVATE kairo_core)

enable_testing()
add_test(NAME layout_test COMMAND layout_test)
add_test(NAME klp_test COMMAND klp_test)
add_test(NAME link_test COMMAND link_test)
add_test(NAME js_test COMMAND js_test)
add_test(NAME js_render_test COMMAND js_render_test)
