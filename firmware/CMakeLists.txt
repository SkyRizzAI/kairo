cmake_minimum_required(VERSION 3.20)
project(kairo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

# Header-only vendor (nlohmann/json) — only used by platform/target, never by core
add_library(kairo_vendor INTERFACE)
target_include_directories(kairo_vendor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

add_subdirectory(core)
add_subdirectory(platforms/simulator)
add_subdirectory(boards/simulator)
add_subdirectory(targets/simulator)
add_subdirectory(tests)
