cmake_minimum_required(VERSION 3.20)
project(kairo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

# WASM (Emscripten) build: firmware uses real threads (Nema kernel) → pthreads.
if(EMSCRIPTEN)
    add_compile_options(-pthread)
    add_link_options(-pthread)
endif()

# Header-only vendor (nlohmann/json) — only used by platform/target, never by core
add_library(kairo_vendor INTERFACE)
target_include_directories(kairo_vendor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

enable_testing()   # root-level so `ctest` discovers host tests (npm run test)

add_subdirectory(vendor/quickjs)   # embedded JS engine (Plan 37)
add_subdirectory(core)
add_subdirectory(boards/simulator)

if(EMSCRIPTEN)
    # Browser/WASM build: firmware runs in a Web Worker, speaks KLP over the
    # virtual cable (postMessage). No stdio sim platform, no host tests.
    add_subdirectory(platforms/wasm)
    add_subdirectory(targets/wasm)
else()
    # Native host build = host unit tests only. The simulator is WASM (Plan 36);
    # the old native `kairo-sim` CLI + stdio platform were removed.
    add_subdirectory(tests)
endif()
