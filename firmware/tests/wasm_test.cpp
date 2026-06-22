// Host test for the WASM engine (Plan 57 / Plan 84 Fase 4).
// Validates the headless run path: module load, import link ordering, WASI
// proc_exit, and exit-code propagation — using a hand-assembled minimal module
// (no wasi-sdk needed in CI). The nema.* bridge is exercised on-device with a
// real toolchain (see plan: that integration test is deferred).
#include "nema/wasm/wasm_engine.h"
#include "nema/proc/process_context.h"
#include "nema/proc/stream.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace nema;

static int fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); fail++; } \
                         else std::printf("  ok:   %s\n", m); } while (0)

// Minimal ProcessContext for headless tests — no Runtime, no thread. runtime()
// is only reached on the error-logging path, which these modules never hit.
class TestCtx : public ProcessContext {
public:
    explicit TestCtx(std::vector<std::string> argv) : argv_(std::move(argv)) {}

    const std::vector<std::string>& args() const override { return argv_; }
    const std::string&              cwd()  const override { return cwd_; }
    const char*                     env(const char*) const override { return nullptr; }

    IInputStream&  in()  override { return in_; }
    IOutputStream& out() override { return out_; }
    IOutputStream& err() override { return err_; }

    void requestExit(int code) override { exit_ = true; code_ = code; }
    bool shouldExit() const override { return exit_; }
    int  exitCode()   const override { return code_; }
    Runtime& runtime() override {
        std::printf("  FAIL: runtime() unexpectedly called in test\n");
        std::abort();
    }

    const std::string& stdoutText() const { return out_.str(); }

private:
    std::vector<std::string> argv_;
    std::string  cwd_ = "/";
    NullInputStream    in_;
    StringOutputStream out_;
    StringOutputStream err_;
    bool exit_ = false;
    int  code_ = 0;
};

// (module
//   (import "wasi_snapshot_preview1" "proc_exit" (func $exit (param i32)))
//   (func $_start  i32.const 7  call $exit)
//   (export "_start" (func $_start)))
static const uint8_t kExit7Wasm[] = {
    0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00,                 // magic + version
    // type section: [0]=(i32)->() , [1]=()->()
    0x01, 0x08, 0x02, 0x60,0x01,0x7f,0x00, 0x60,0x00,0x00,
    // import section: wasi_snapshot_preview1.proc_exit : type 0
    0x02, 0x24, 0x01,
      0x16, 0x77,0x61,0x73,0x69,0x5f,0x73,0x6e,0x61,0x70,0x73,0x68,0x6f,
            0x74,0x5f,0x70,0x72,0x65,0x76,0x69,0x65,0x77,0x31,
      0x09, 0x70,0x72,0x6f,0x63,0x5f,0x65,0x78,0x69,0x74,
      0x00, 0x00,
    // function section: 1 func of type 1
    0x03, 0x02, 0x01, 0x01,
    // export section: "_start" -> func 1
    0x07, 0x0a, 0x01, 0x06, 0x5f,0x73,0x74,0x61,0x72,0x74, 0x00, 0x01,
    // code section: _start = { i32.const 7; call 0; end }
    0x0a, 0x08, 0x01, 0x06, 0x00, 0x41,0x07, 0x10,0x00, 0x0b,
};

int main() {
    std::printf("== WasmEngine tests ==\n");

    // 1. Load + run a real module: proc_exit(7) → exit code 7.
    {
        WasmEngine eng;
        CHECK(eng.init(64 * 1024), "engine init");
        CHECK(eng.load(kExit7Wasm, sizeof(kExit7Wasm)), "load minimal _start module");
        CHECK(eng.loaded(), "module reports loaded");

        TestCtx ctx({"exit7"});
        int code = eng.runStart(ctx, "com.test.exit7");
        CHECK(ctx.shouldExit(), "guest requested exit via WASI proc_exit");
        CHECK(code == 7, "exit code 7 propagated from proc_exit");
    }

    // 2. Garbage bytes fail to load, no crash, error captured.
    {
        WasmEngine eng;
        CHECK(eng.init(64 * 1024), "engine init (negative)");
        const uint8_t junk[] = {0xde,0xad,0xbe,0xef,0x01,0x02,0x03,0x04};
        CHECK(!eng.load(junk, sizeof(junk)), "garbage module rejected");
        CHECK(!eng.loaded(), "engine not loaded after bad parse");
        CHECK(!eng.lastError().empty(), "parse error captured");
    }

    std::printf(fail == 0 ? "== ALL PASS ==\n" : "== FAILURES ==\n");
    return fail == 0 ? 0 : 1;
}
