add_executable(palanu main.cpp)
target_link_libraries(palanu PRIVATE nema_core nema_platform_wasm nema_board_sim)

# Emit a CLASSIC script (palanu.js + palanu.wasm). Loaded via a <script> tag on the
# page (outside Vite's ESM transform, which would mangle import.meta.url and break
# pthread workers). emscripten manages its own pthread pool from the page thread.
set_target_properties(palanu PROPERTIES SUFFIX ".js")

target_link_options(palanu PRIVATE
    -sENVIRONMENT=web,worker
    # Palanu stays multi-threaded in WASM (pthreads = Web Workers). Fixed shared
    # memory (NO growth): growable SharedArrayBuffer + pthreads is buggy and is a
    # known cause of the pthread-pool "loading-workers" hang. main() unwinds via
    # emscripten_set_main_loop(simulate_infinite_loop) so the event loop can
    # service worker startup (no PROXY → exports remain callable for the cable).
    -sINITIAL_MEMORY=536870912
    -sPTHREAD_POOL_SIZE=12
    # Emscripten's default thread stack is only 64 KB (STACK_SIZE) — far too small
    # for the QuickJS JS-app engine, which needs a few hundred KB to load+run a
    # module. Give every pthread a 1 MB stack so the app worker has room; JsApp
    # keeps QuickJS's overflow guard well below this (clean error, never a crash).
    -sDEFAULT_PTHREAD_STACK_SIZE=1048576
    -sSTACK_SIZE=1048576
    "SHELL:-s EXPORTED_FUNCTIONS=['_main','_nema_plp_recv','_malloc','_free']"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['HEAPU8','ccall','cwrap']"
)
