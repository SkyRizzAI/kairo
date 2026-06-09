add_executable(kairo main.cpp)
target_link_libraries(kairo PRIVATE kairo_core kairo_platform_wasm kairo_board_sim)

# Emit a CLASSIC script (kairo.js + kairo.wasm). Loaded via a <script> tag on the
# page (outside Vite's ESM transform, which would mangle import.meta.url and break
# pthread workers). emscripten manages its own pthread pool from the page thread.
set_target_properties(kairo PROPERTIES SUFFIX ".js")

target_link_options(kairo PRIVATE
    -sENVIRONMENT=web,worker
    # Kairo stays multi-threaded in WASM (pthreads = Web Workers). Fixed shared
    # memory (NO growth): growable SharedArrayBuffer + pthreads is buggy and is a
    # known cause of the pthread-pool "loading-workers" hang. main() unwinds via
    # emscripten_set_main_loop(simulate_infinite_loop) so the event loop can
    # service worker startup (no PROXY → exports remain callable for the cable).
    -sINITIAL_MEMORY=536870912
    -sPTHREAD_POOL_SIZE=12
    "SHELL:-s EXPORTED_FUNCTIONS=['_main','_kairo_klp_recv','_malloc','_free']"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['HEAPU8','ccall','cwrap']"
)
