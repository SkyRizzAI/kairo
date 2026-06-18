# IDF component wrapper for wasm3 WASM interpreter (nema Plan 57).
# The upstream wasm3 CMakeLists.txt builds a standalone CLI — incompatible
# with IDF's component model. This thin wrapper exposes the m3 core source
# as an IDF component usable via `REQUIRES wasm3_idf`.
set(wasm3_dir "${CMAKE_CURRENT_LIST_DIR}/../wasm3")

idf_component_register(
    SRCS
        "${wasm3_dir}/source/m3_api_libc.c"
        "${wasm3_dir}/source/m3_api_meta_wasi.c"
        "${wasm3_dir}/source/m3_api_tracer.c"
        "${wasm3_dir}/source/m3_api_uvwasi.c"
        "${wasm3_dir}/source/m3_api_wasi.c"
        "${wasm3_dir}/source/m3_bind.c"
        "${wasm3_dir}/source/m3_code.c"
        "${wasm3_dir}/source/m3_compile.c"
        "${wasm3_dir}/source/m3_core.c"
        "${wasm3_dir}/source/m3_env.c"
        "${wasm3_dir}/source/m3_exec.c"
        "${wasm3_dir}/source/m3_function.c"
        "${wasm3_dir}/source/m3_info.c"
        "${wasm3_dir}/source/m3_module.c"
        "${wasm3_dir}/source/m3_parse.c"
    INCLUDE_DIRS
        "${wasm3_dir}/source"
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE d_m3HasTracer)
# ESP-IDF has -Werror globally; upstream wasm3 has benign warnings we cannot fix.
# Suppress -Werror for this component only.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)
