// Plan 84 Fase 4 — nema.* host imports for WASM apps.
// Mirrors the JS host surface (nema_host_impl.cpp) for the wasm3 tier: log,
// device info, and namespaced app storage. WASI (wasm_wasi.cpp) covers stdio
// and argv; this file covers the Nema system API.
//
// Strings cross the boundary as wasm linear-memory offsets to NUL-terminated
// C strings; we resolve + bounds-check them against the guest memory before use.

#include "nema/wasm/wasm_engine.h"
#include "nema/proc/process_context.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/system/capability_registry.h"
#include "nema/system/system_info.h"
#include "nema/fs/app_storage.h"
#include "wasm3.h"
#include "m3_env.h"
#include <cstring>
#include <string>

namespace nema {
namespace {

// ── Helpers ────────────────────────────────────────────────────────────────

static WasmHostCtx* hostOf(IM3Runtime rt) {
    return static_cast<WasmHostCtx*>(m3_GetUserData(rt));
}

// Resolve a guest pointer to a host C-string, NUL-terminated within guest
// memory bounds. Returns false (and leaves out empty) if the offset is out of
// range or the string is unterminated before the memory end.
static bool readCStr(IM3Runtime rt, uint32_t off, std::string& out) {
    uint32_t memSize = 0;
    uint8_t* base = m3_GetMemory(rt, &memSize, 0);
    if (!base || off >= memSize) return false;
    uint32_t end = off;
    while (end < memSize && base[end] != 0) end++;
    if (end >= memSize) return false;   // unterminated
    out.assign(reinterpret_cast<const char*>(base + off), end - off);
    return true;
}

// Copy a host string into guest memory at `off`, up to `cap` bytes including
// the NUL. Returns the number of bytes written (excluding NUL), or -1 on bounds
// failure. Truncates if the source is longer than the buffer.
static int writeBuf(IM3Runtime rt, uint32_t off, int cap, const std::string& src) {
    if (cap <= 0) return -1;
    uint32_t memSize = 0;
    uint8_t* base = m3_GetMemory(rt, &memSize, 0);
    if (!base || off >= memSize || off + (uint32_t)cap > memSize) return -1;
    int n = (int)src.size();
    if (n > cap - 1) n = cap - 1;       // reserve a byte for the NUL
    std::memcpy(base + off, src.data(), (size_t)n);
    base[off + n] = 0;
    return n;
}

// ── nema.log(level, tag, msg) ──────────────────────────────────────────────

m3ApiRawFunction(nema_log) {
    m3ApiGetArg(uint32_t, levelOff);
    m3ApiGetArg(uint32_t, tagOff);
    m3ApiGetArg(uint32_t, msgOff);

    WasmHostCtx* h = hostOf(runtime);
    if (!h || !h->ctx) m3ApiSuccess();

    std::string level, tag, msg;
    readCStr(runtime, levelOff, level);
    readCStr(runtime, tagOff,   tag);
    readCStr(runtime, msgOff,   msg);

    auto& log = h->ctx->runtime().log();
    const char* t = tag.c_str();
    const char* m = msg.c_str();
    if      (level == "error") log.error(t, m);
    else if (level == "warn")  log.warn (t, m);
    else if (level == "debug") log.debug(t, m);
    else if (level == "trace") log.trace(t, m);
    else if (level == "fatal") log.fatal(t, m);
    else                       log.info (t, m);

    m3ApiSuccess();
}

// ── nema.device_name(out, len) → bytes written ─────────────────────────────

m3ApiRawFunction(nema_device_name) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, outOff);
    m3ApiGetArg(int32_t, cap);

    WasmHostCtx* h = hostOf(runtime);
    if (!h || !h->ctx) m3ApiReturn(-1);

    m3ApiReturn(writeBuf(runtime, outOff, cap, h->ctx->runtime().info().boardName));
}

// ── nema.device_caps(out, len) → bytes written (newline-separated) ─────────

m3ApiRawFunction(nema_device_caps) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, outOff);
    m3ApiGetArg(int32_t, cap);

    WasmHostCtx* h = hostOf(runtime);
    if (!h || !h->ctx) m3ApiReturn(-1);

    std::string joined;
    for (const auto& c : h->ctx->runtime().capabilities().list()) {
        if (!joined.empty()) joined += '\n';
        joined += c;
    }
    m3ApiReturn(writeBuf(runtime, outOff, cap, joined));
}

// ── nema.storage_fs_read_file(name, out, cap) → bytes read, -1 on miss ─────

m3ApiRawFunction(nema_storage_fs_read_file) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, nameOff);
    m3ApiGetArg(uint32_t, outOff);
    m3ApiGetArg(int32_t, cap);

    WasmHostCtx* h = hostOf(runtime);
    if (!h || !h->ctx) m3ApiReturn(-1);

    std::string name;
    if (!readCStr(runtime, nameOff, name)) m3ApiReturn(-1);

    Runtime& rt = h->ctx->runtime();
    AppStorage stor(h->appId, rt.fs(), rt.config(), false);
    std::vector<uint8_t> buf;
    if (!stor.read(name.c_str(), buf)) m3ApiReturn(-1);

    std::string data(buf.begin(), buf.end());
    m3ApiReturn(writeBuf(runtime, outOff, cap, data));
}

// ── nema.storage_fs_write_file(name, data, len) → 0 ok, -1 fail ────────────

m3ApiRawFunction(nema_storage_fs_write_file) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, nameOff);
    m3ApiGetArgMem(const uint8_t*, data);
    m3ApiGetArg(int32_t, len);

    WasmHostCtx* h = hostOf(runtime);
    if (!h || !h->ctx) m3ApiReturn(-1);
    if (len < 0) m3ApiReturn(-1);
    m3ApiCheckMem(data, (uint32_t)len);

    std::string name;
    if (!readCStr(runtime, nameOff, name)) m3ApiReturn(-1);

    Runtime& rt = h->ctx->runtime();
    AppStorage stor(h->appId, rt.fs(), rt.config(), false);
    bool ok = stor.write(name.c_str(), data, (size_t)len);
    m3ApiReturn(ok ? 0 : -1);
}

} // anon namespace

// ── Public API ─────────────────────────────────────────────────────────────

void linkNemaImports(IM3Module mod) {
    // Userdata is set by WasmEngine::runStart() before this is called.
    auto link = [mod](const char* fn, const char* sig, M3RawCall trampoline) {
        m3_LinkRawFunction(mod, "nema", fn, sig, trampoline);
    };

    link("log",                   "v(***)",  &nema_log);
    link("device_name",           "i(*i)",   &nema_device_name);
    link("device_caps",           "i(*i)",   &nema_device_caps);
    link("storage_fs_read_file",  "i(**i)",  &nema_storage_fs_read_file);
    link("storage_fs_write_file", "i(**i)",  &nema_storage_fs_write_file);
}

} // namespace nema
