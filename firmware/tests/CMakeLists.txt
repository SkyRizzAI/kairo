# Host unit tests for hardware-agnostic core logic.
add_executable(layout_test layout_test.cpp)
target_link_libraries(layout_test PRIVATE kairo_core)

enable_testing()
add_test(NAME layout_test COMMAND layout_test)
