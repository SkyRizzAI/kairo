# Vendored QuickJS-ng (MIT) — embedded JS engine for Palanu custom apps (Plan 37).
# Core only: no quickjs-libc (no FS/stdio). Dual-build: host/WASM (plain CMake)
# and ESP-IDF (component). Warnings disabled (3rd-party C vs our -Wall/-Werror).
set(QUICKJS_SRCS quickjs.c dtoa.c libregexp.c libunicode.c)

if(ESP_PLATFORM)
    idf_component_register(SRCS ${QUICKJS_SRCS} INCLUDE_DIRS ".")
    set_target_properties(${COMPONENT_LIB} PROPERTIES C_STANDARD 11)
    # 3rd-party C: silence warnings AND neutralise IDF's -Werror (xtensa types
    # int32_t==long trip -Werror=incompatible-pointer-types in QuickJS Atomics;
    # benign on 32-bit). -Wno-error is appended last so it wins over IDF flags.
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -w
        -Wno-error=incompatible-pointer-types
        -Wno-incompatible-pointer-types)
else()
    add_library(quickjs STATIC ${QUICKJS_SRCS})
    set_target_properties(quickjs PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
    target_include_directories(quickjs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(quickjs PRIVATE -w)
    find_package(Threads QUIET)
    if(Threads_FOUND)
        target_link_libraries(quickjs PUBLIC Threads::Threads)
    endif()
endif()
