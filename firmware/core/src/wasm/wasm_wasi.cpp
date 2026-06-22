// Plan 57 Fase 2 — WASI imports delegating to Nema ProcessContext.
// Implements the minimal WASI surface: fd 0/1/2, argv, exit.
// Built-in wasm3 WASI is disabled; we own the mapping.

#include "nema/wasm/wasm_engine.h"
#include "nema/proc/process_context.h"
#include "nema/proc/stream.h"
#include "wasm3.h"
#include "m3_env.h"
#include <cstring>

namespace nema {
namespace {

// ── Helpers ────────────────────────────────────────────────────────────────

static ProcessContext* ctxOf(IM3Runtime rt) {
    auto* h = static_cast<WasmHostCtx*>(m3_GetUserData(rt));
    return h ? h->ctx : nullptr;
}

static bool boundsOk(IM3Runtime rt, uintptr_t off, uint32_t len) {
    uint32_t memSize = m3_GetMemorySize(rt);
    if (off > memSize || len > memSize || (off + len) > memSize) return false;
    return true;
}

static uint8_t* memPtr(IM3Runtime rt, uintptr_t off) {
    uint32_t memSize = 0;
    return m3_GetMemory(rt, &memSize, (uint32_t)off);
}

// ── WASI: args_sizes_get ──────────────────────────────────────────────────

m3ApiRawFunction(wasi_args_sizes_get) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArgMem(uint32_t*, argcPtr);
    m3ApiGetArgMem(uint32_t*, argvBufSizePtr);
    m3ApiCheckMem(argcPtr, 4);
    m3ApiCheckMem(argvBufSizePtr, 4);

    ProcessContext* ctx = ctxOf(runtime);
    if (!ctx) m3ApiTrap(m3Err_trapOutOfBoundsMemoryAccess);

    uint32_t totalLen = 0;
    for (auto& a : ctx->args()) totalLen += (uint32_t)a.size() + 1;

    *argcPtr      = (uint32_t)ctx->args().size();
    *argvBufSizePtr = totalLen;

    m3ApiReturn(0);
}

// ── WASI: args_get ────────────────────────────────────────────────────────

m3ApiRawFunction(wasi_args_get) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArgMem(uint32_t*, argvOffs);
    m3ApiGetArgMem(uint8_t*,  buf);

    ProcessContext* ctx = ctxOf(runtime);
    if (!ctx) m3ApiTrap(m3Err_trapOutOfBoundsMemoryAccess);

    const auto& args = ctx->args();
    uint32_t totalLen = 0;
    for (const auto& a : args) totalLen += (uint32_t)a.size() + 1;

    m3ApiCheckMem(argvOffs, (uint32_t)(args.size() * 4));
    m3ApiCheckMem(buf, totalLen);

    uint32_t off = 0;
    for (size_t i = 0; i < args.size(); i++) {
        argvOffs[i] = off;
        std::memcpy(buf + off, args[i].c_str(), args[i].size() + 1);
        off += (uint32_t)args[i].size() + 1;
    }

    m3ApiReturn(0);
}

// ── WASI: fd_write ────────────────────────────────────────────────────────

m3ApiRawFunction(wasi_fd_write) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(const uint32_t*, iovOffs);
    m3ApiGetArg(int32_t, iovLen);
    m3ApiGetArgMem(uint32_t*, nwritten);

    ProcessContext* ctx = ctxOf(runtime);
    if (!ctx) m3ApiTrap(m3Err_trapOutOfBoundsMemoryAccess);

    m3ApiCheckMem(iovOffs, (uint32_t)(iovLen * 8));
    m3ApiCheckMem(nwritten, 4);

    IOutputStream* out = (fd == 2) ? &ctx->err() : &ctx->out();
    uint32_t written = 0;

    for (int32_t i = 0; i < iovLen; i++) {
        uint32_t bufOff = iovOffs[i * 2];
        uint32_t bufLen = iovOffs[i * 2 + 1];
        m3ApiCheckMem((void*)(uintptr_t)bufOff, bufLen);
        out->write(memPtr(runtime, bufOff), bufLen);
        written += bufLen;
    }

    out->flush();
    *nwritten = written;
    m3ApiReturn(0);
}

// ── WASI: fd_read ─────────────────────────────────────────────────────────

m3ApiRawFunction(wasi_fd_read) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(const uint32_t*, iovOffs);
    m3ApiGetArg(int32_t, iovLen);
    m3ApiGetArgMem(uint32_t*, nread);

    ProcessContext* ctx = ctxOf(runtime);
    if (!ctx) m3ApiTrap(m3Err_trapOutOfBoundsMemoryAccess);
    if (fd != 0) m3ApiTrap(m3Err_trapOutOfBoundsMemoryAccess);

    m3ApiCheckMem(iovOffs, (uint32_t)(iovLen * 8));
    m3ApiCheckMem(nread, 4);

    IInputStream& in = ctx->in();
    uint32_t total = 0;

    for (int32_t i = 0; i < iovLen; i++) {
        uint32_t bufOff = iovOffs[i * 2];
        uint32_t bufLen = iovOffs[i * 2 + 1];
        m3ApiCheckMem((void*)(uintptr_t)bufOff, bufLen);

        int n = in.read(memPtr(runtime, bufOff), bufLen);
        if (n < 0) continue;
        total += (uint32_t)n;
        if (n == 0 && in.eof()) break;
        if ((uint32_t)n < bufLen) break;
    }

    *nread = total;
    m3ApiReturn(0);
}

// ── WASI: proc_exit ───────────────────────────────────────────────────────

m3ApiRawFunction(wasi_proc_exit) {
    m3ApiGetArg(int32_t, code);

    ProcessContext* ctx = ctxOf(runtime);
    if (ctx) ctx->requestExit(code);

    m3ApiTrap(m3Err_trapExit);
}

} // anon namespace

// ── Public API ─────────────────────────────────────────────────────────────

void linkWasiImports(IM3Module mod) {
    // Userdata is set by WasmEngine::runStart() before this is called.
    auto link = [mod](const char* fn, const char* sig, M3RawCall trampoline) {
        m3_LinkRawFunction(mod, "wasi_snapshot_preview1", fn, sig, trampoline);
    };

    link("args_sizes_get",   "i(ii)", &wasi_args_sizes_get);
    link("args_get",         "i(ii)", &wasi_args_get);
    link("fd_write",         "i(i*i)", &wasi_fd_write);
    link("fd_read",          "i(i*i)", &wasi_fd_read);
    link("proc_exit",        "v(i)",  &wasi_proc_exit);
}

} // namespace nema
