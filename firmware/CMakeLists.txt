cmake_minimum_required(VERSION 3.20)
project(nema LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

# WASM (Emscripten) build: firmware uses real threads (Nema kernel) → pthreads.
if(EMSCRIPTEN)
    add_compile_options(-pthread)
    add_link_options(-pthread)
endif()

# Header-only vendor (nlohmann/json) — used by core (manifest parsing) and platform/target
add_library(nema_vendor INTERFACE)
target_include_directories(nema_vendor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

enable_testing()   # root-level so `ctest` discovers host tests (npm run test)

add_subdirectory(vendor/quickjs)   # embedded JS engine (Plan 37)

# wasm3 — WASM interpreter (Plan 57). Built as a STATIC library from source
# instead of using its own CMakeLists.txt, which would create an EXECUTABLE
# (m3 CLI) and use project() — incompatible with add_subdirectory on host and
# ESP-IDF component builds.
add_library(wasm3 STATIC
    vendor/wasm3/source/m3_api_libc.c
    vendor/wasm3/source/m3_api_meta_wasi.c
    vendor/wasm3/source/m3_api_tracer.c
    vendor/wasm3/source/m3_api_uvwasi.c
    vendor/wasm3/source/m3_api_wasi.c
    vendor/wasm3/source/m3_bind.c
    vendor/wasm3/source/m3_code.c
    vendor/wasm3/source/m3_compile.c
    vendor/wasm3/source/m3_core.c
    vendor/wasm3/source/m3_env.c
    vendor/wasm3/source/m3_exec.c
    vendor/wasm3/source/m3_function.c
    vendor/wasm3/source/m3_info.c
    vendor/wasm3/source/m3_module.c
    vendor/wasm3/source/m3_parse.c
)
target_include_directories(wasm3 PUBLIC vendor/wasm3/source)
target_compile_definitions(wasm3 PRIVATE d_m3HasTracer)
add_subdirectory(core)
add_subdirectory(servers/fbcon)   # FbCon text-console server (Plan 80)
add_subdirectory(aether)   # display-server module (Plan 80) — depends on core
add_subdirectory(boards/simulator)

if(EMSCRIPTEN)
    # Browser/WASM build: firmware runs in a Web Worker, speaks PLP over the
    # virtual cable (postMessage). No stdio sim platform, no host tests.
    add_subdirectory(platforms/wasm)
    add_subdirectory(targets/wasm)
else()
    # Native host build = host unit tests only. The simulator is WASM (Plan 36);
    # the old native `nema-sim` CLI + stdio platform were removed.
    add_subdirectory(tests)
endif()
