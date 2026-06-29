#include "nema/app/app_host.h"
#include "nema/app/app.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/ui/canvas.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/hal/buffer_display.h"
#include "nema/hal/display.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include "nema/event/event.h"
#include "nema/event/async_event_poster.h"
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef ESP_PLATFORM
  #include <esp_heap_caps.h>
#endif

namespace nema {

// preferInternal: put hot buffers (read every frame during blit) in fast
// internal SRAM — reading a frame buffer out of PSRAM 76,800×/frame is a major
// perf trap. Falls back to PSRAM then plain malloc if internal won't fit.
static uint8_t* allocBuf(size_t n, bool preferInternal = false) {
#ifdef ESP_PLATFORM
    if (preferInternal)
        if (auto* p = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))
            return p;
    if (auto* p = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM)) return p;
    return (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_8BIT);
#else
    (void)preferInternal;
    return (uint8_t*)std::malloc(n);
#endif
}
static void freeBuf(uint8_t* p) {
#ifdef ESP_PLATFORM
    if (p) heap_caps_free(p);
#else
    std::free(p);
#endif
}

AppHost::AppHost(Runtime& rt, IApp& app, std::vector<std::string> args)
    : rt_(rt), app_(app), args_(std::move(args))
{
    termOut_ = std::make_unique<LineOutputStream>([this](const std::string& line) {
        terminalLines_.push_back(line);
        if (hostMode_ == HostMode::Terminal) { rt_.view().requestRedraw(); rt_.guiWaker().signal(); }
    });
    termErr_ = std::make_unique<LineOutputStream>([this](const std::string& line) {
        terminalLines_.push_back("[err] " + line);
        if (hostMode_ == HostMode::Terminal) { rt_.view().requestRedraw(); rt_.guiWaker().signal(); }
    });
    // Plan 97 — opt-in latency instrumentation (off unless aether/applatency=1).
    if (auto* cfg = rt_.container().resolve<IConfigStore>())
        latencyCfg_ = cfg->getIntOr("aether", "applatency", 0) != 0;
}

AppHost::~AppHost() {
    delete appCanvas_;
    delete bufDisplay_;
    freeBuf(drawBuf_);
    freeBuf(readyBuf_);
}

// ── GUI thread ────────────────────────────────────────────────────────────

void AppHost::onResume() {
    if (started_) {
        // Re-enter. Either revealed by a child screen pop (no-op), or resumed
        // from pause (Plan 22) — clear the pause flag so the parked app thread
        // wakes from waitInput() and repaints.
        if (paused_.load()) { paused_.store(false); rt_.view().requestRedraw(); }
        return;
    }
    w_    = rt_.canvas().width();
    h_    = rt_.canvas().height();
    size_ = BufferDisplay::byteSize(w_, h_);   // Plan 97 P3b: 1-bit packed (~8× smaller)
    // Plan 97 — lazy framebuffer alloc. The two full-screen buffers (~150 KB PSRAM)
    // are NOT allocated here: a terminal/CLI app (BadUSB, shells, compute-only
    // WASM/JS) never draws and would otherwise waste them. ensureCanvas() allocates
    // them on first canvas()/present() (i.e. when the app actually goes GUI).
    display_    = rt_.container().resolve<IDisplayDriver>();  // for fast fullscreen path

    started_  = true;
    finished_ = false;
    rt_.log().info("AppHost", "enter", {{"w", std::to_string(w_)}, {"h", std::to_string(h_)}});
    // Pre-resolve storage paths on this (GUI / internal-RAM) thread before the
    // app thread starts. JS/WASM app threads use PSRAM stacks; NVS flash reads
    // disable the SPI cache which also disables PSRAM → assert if called there.
    warmStorage();
    // App thread on core 0 (off the GUI/core-1) by default. May block freely.
    // Stack size comes from the app: JS apps need more (QuickJS recurses deeply).
    //
    // Plan 97 P0b — core/priority are tunable so the app-vs-WiFi/BLE core-0 jitter
    // can be A/B tested on hardware WITHOUT changing the default. Defaults reproduce
    // the original {prio 5, core 0}. e.g. set config app/thread_core=1 to move the
    // app onto the GUI core (frees core 0 for radio); measure radio + responsiveness
    // before adopting. -1 = no affinity.
    uint8_t prio = 5;
    int8_t  core = 0;
    if (auto* cfg = rt_.container().resolve<IConfigStore>()) {
        prio = (uint8_t)cfg->getIntOr("app", "thread_prio", prio);
        core = (int8_t) cfg->getIntOr("app", "thread_core", core);
    }
    thread_.start({"nema_app", app_.stackBytes(), prio, core}, &AppHost::threadEntry, this);
    rt_.view().requestRedraw();
}

void AppHost::update(Key key) {
    // Legacy/direct path (e.g. DPM handleKey). Populate code+action from the key
    // so the forwarded event is self-consistent.
    InputEvent ie;
    ie.kind   = InputEvent::Kind::Key;
    ie.key    = key;
    ie.code   = input::codeFromKey(key);
    ie.action = input::defaultAction(ie.code);
    ie.type   = InputEvent::Type::Press;
    mailbox_.send(ie);
}

// Primary input path. GuiService calls onAction() immediately followed by
// onCode() for every press (gui_service.cpp). We buffer the board-resolved
// action here and emit the complete event in onCode() — carrying the true
// PHYSICAL key via keyFromCode() instead of the lossy keyFromAction() round-trip
// the base IScreen::onAction() used to apply.
void AppHost::onAction(input::Action a) {
    pendingAction_ = a;
}

void AppHost::onCode(input::Code c) {
    if (c == input::Code::None) { pendingAction_ = input::Action::None; return; }
    InputEvent ie;
    ie.kind   = InputEvent::Kind::Key;
    ie.type   = InputEvent::Type::Press;
    ie.code   = c;
    ie.key    = input::keyFromCode(c);   // lossless: physical direction preserved
    ie.action = pendingAction_;          // board-resolved intent (None if unpaired)
    pendingAction_ = input::Action::None;
    if (latencyOn()) lastInputMs_.store(rt_.clock().millis());
    mailbox_.send(ie);
}

void AppHost::onPointer(const input::PointerEvent& e) {
    InputEvent ie;
    ie.kind   = InputEvent::Kind::Pointer;
    ie.type   = InputEvent::Type::Press;
    ie.pphase = e.phase;
    ie.px = e.x; ie.py = e.y;
    if (latencyOn()) lastInputMs_.store(rt_.clock().millis());
    mailbox_.send(ie);   // delivered to app thread via waitInput()
}

ScreenMode AppHost::mode() const {
    return app_.fullscreen() ? ScreenMode::Fullscreen : ScreenMode::Normal;
}

void AppHost::draw(Canvas& c) {
    std::lock_guard<std::mutex> lk(frameMtx_);
    if (dbgDraw_ < 6) {
        rt_.log().info("AppHost", "draw", {{"n", std::to_string(dbgDraw_)},
                                           {"hasFrame", hasFrame_ ? "1" : "0"}});
        dbgDraw_++;
    }
    if (!hasFrame_) return;
    drawnSeq_ = frameSeq_.load(std::memory_order_acquire);   // this frame is being drawn

    // Plan 97 — latency: full input→pixel, plus the present→pixel handoff that
    // P0+P1 (event-driven GUI) collapsed from up-to-33 ms to ~0.
    if (latencyOn()) {
        if (uint64_t in = pendingInputMs_.exchange(0)) {
            uint64_t now = rt_.clock().millis();
            rt_.log().info("AppLatency", "in→pixel",
                {{"ms", std::to_string(now - in)},
                 {"present→pixel", std::to_string(now - presentMs_.load())}});
        }
    }

    // Fullscreen fast path: push the whole frame straight to the panel in one
    // pass (skips 76,800 per-pixel drawPixel calls). Only valid when the app
    // buffer is at physical dimensions (scale == 1.0); at scale > 1.0, w_/h_
    // are logical (smaller) and flushBuffer would silently no-op.
    //
    // ONLY when topmost: if a system modal (permission / wallet sign-consent) is pushed
    // above us, a direct flushBuffer would overwrite the whole panel every frame while
    // the modal renders into the canvas → the two full-panel writes alternate and the
    // modal flickers. With a modal above, fall through to the canvas blit so the modal
    // composites onto a single flushed frame.
    if (rt_.view().active() == this && app_.fullscreen() && display_ &&
        w_ == display_->width() && h_ == display_->height()) {
        display_->flushBuffer(readyBuf_, w_, h_);
        return;
    }

    // Scaled fullscreen or normal mode: blit via canvas drawPixel.
    // Fullscreen apps own the whole canvas; clear first so physical-pixel gaps
    // left by non-integer scale don't show stale content at the edges.
    if (app_.fullscreen()) c.clear(false);
    uint16_t top = app_.fullscreen() ? 0 : (uint16_t)(nema::display::SEP1_Y + 2);
    for (uint16_t y = top; y < h_; y++)
        for (uint16_t x = 0; x < w_; x++)
            c.drawPixel(x, y, nema::mono1::get(readyBuf_, w_, x, y));  // Plan 97 P3b
}

bool AppHost::suppressCanvasFlush() const {
    // Suppress only when we actually took the flushBuffer fast path (physical
    // dimensions match) AND we're topmost. With a modal above us we render into the
    // canvas (see draw()), so GuiService must flush the canvas normally — otherwise
    // the modal would never reach the panel.
    return rt_.view().active() == this && app_.fullscreen() && display_ != nullptr &&
           w_ == display_->width() && h_ == display_->height();
}

void AppHost::tick(uint64_t) {
    if (started_ && !finished_ && !thread_.running()) {
        finished_ = true;
        thread_.join();
        rt_.view().pop();    // app exited → return to previous screen
        return;
    }
    // Safety net: if the app published a newer frame than we've drawn, make sure
    // the GUI loop renders again. Covers the case where the present()-time
    // requestRedraw raced ahead of the first draw (seen on WASM: a freshly loaded
    // app stayed blank until some other event forced a redraw).
    if (started_ && frameSeq_.load(std::memory_order_acquire) != drawnSeq_)
        rt_.view().requestRedraw();
}

// ── App thread ──────────────────────────────────────────────────────────────

void AppHost::threadEntry(void* self) {
    auto* h = static_cast<AppHost*>(self);
    int exitCode = 0;
    try {
        h->app_.run(*h);
    } catch (const std::exception& e) {
        h->rt_.log().error("AppHost", "app crashed",
            {{"app", h->app_.name()}, {"what", e.what()}});
        exitCode = 1;
    } catch (...) {
        h->rt_.log().error("AppHost", "app crashed (unknown)",
            {{"app", h->app_.name()}});
        exitCode = 1;
    }
    // Always emit so lease/resource holders can auto-release on any exit type.
    // AppHostManager::onAppExited already handles exitCode==0 cleanly (erases crash map).
    h->rt_.asyncPoster().post({events::AppHostExited, {
        {"id",       std::string(h->app_.id())},
        {"name",     h->app_.name()},
        {"exitCode", std::to_string(exitCode)}}});
}

// Allocate the two full-screen frame buffers + Canvas on first use (app thread).
// Idempotent. PSRAM, not internal: a 240×320 buffer is ~75 KB; forcing it internal
// was THE cause of internal-RAM exhaustion (free internal → ~18 KB) once an app ran,
// which made 8 KB task-stack allocations and SPI-DMA priv buffers fail (device
// reboot). The fullscreen present path (display_->flushBuffer) reads this once per
// frame into the LCD driver's own DMA staging buffer — fine from PSRAM on the S3.
void AppHost::ensureCanvas() {
    if (appCanvas_) return;
    drawBuf_  = allocBuf(size_);   // written by app thread (PSRAM ok)
    readyBuf_ = allocBuf(size_);
    std::memset(drawBuf_,  0, size_);
    std::memset(readyBuf_, 0, size_);
    bufDisplay_ = new BufferDisplay(drawBuf_, w_, h_);
    appCanvas_  = new Canvas(*bufDisplay_);
}

Canvas& AppHost::canvas() { ensureCanvas(); return *appCanvas_; }

// Plan 97 — latency logging is on when explicitly configured OR when the FPS
// overlay is enabled (the toggle the user already controls for "show me perf").
bool AppHost::latencyOn() const { return latencyCfg_ || rt_.showFps(); }

void AppHost::present() {
    ensureCanvas();   // Plan 97: an app that present()s without canvas() still needs buffers
    // Diagnostic: on the first few presents, count non-zero pixels in drawBuf_
    // so we can tell whether the app renderer actually drew anything visible.
    if (dbgPresent_ < 4) {
        size_t nonzero = 0;
        for (size_t i = 0; i < size_; i++) if (drawBuf_[i]) nonzero++;
        rt_.log().info("AppHost", "present",
                       {{"n",  std::to_string(dbgPresent_)},
                        {"nz", std::to_string(nonzero)},
                        {"sz", std::to_string(size_)}});
        dbgPresent_++;
    }
    {
        std::lock_guard<std::mutex> lk(frameMtx_);
        std::memcpy(readyBuf_, drawBuf_, size_);
        hasFrame_ = true;
    }
    frameSeq_.fetch_add(1, std::memory_order_release);
    // Plan 97 — latency: time from input delivery to this app frame being produced
    // (app reaction time, includes core-0 scheduling jitter vs WiFi/BLE — P0b).
    if (latencyOn()) {
        uint64_t now = rt_.clock().millis();
        presentMs_.store(now);
        if (uint64_t in = lastInputMs_.exchange(0)) {
            pendingInputMs_.store(in);
            rt_.log().info("AppLatency", "in→present",
                {{"ms", std::to_string(now - in)}, {"app", app_.name()}});
        }
    }
    rt_.view().requestRedraw();   // ask GUI thread to re-blit
    rt_.guiWaker().signal();      // Plan 97: wake it NOW (no +1-frame poll wait)
}

bool AppHost::nextInput(InputEvent& out) {
    if (paused_.load()) return false;   // swallow input while paused
    return mailbox_.tryReceive(out);
}
bool AppHost::waitInput(InputEvent& out, uint32_t timeoutMs) {
    // While paused (Plan 22), park here at ~0 CPU — stack/state preserved — until
    // resumed (enter() clears the flag) or killed (requestExit()).
    while (paused_.load() && !shouldExit())
        nema::Thread::sleepMs(20);
    if (shouldExit()) return false;
    return mailbox_.receive(out, timeoutMs);
}

const char* AppHost::appName() const { return app_.name(); }

// ── ProcessContext impl (Plan 54) ──────────────────────────────────────────

const std::vector<std::string>& AppHost::args() const { return args_; }
const std::string&              AppHost::cwd()  const { return cwd_; }
const char*                     AppHost::env(const char*) const { return nullptr; }
IInputStream&  AppHost::in()  { return nullIn_; }
IOutputStream& AppHost::out() { return *termOut_; }
IOutputStream& AppHost::err() { return *termErr_; }

void AppHost::enterGuiMode() {
    if (hostMode_ != HostMode::Gui) {
        hostMode_ = HostMode::Gui;
        rt_.log().info("AppHost", "gui mode", {{"app", app_.name()}});
    }
}

void AppHost::requestExit(int code) {
    exitCode_ = code;
    thread_.requestStop();
    paused_.store(false);
}

void AppHost::forceQuit() {
    exitCode_ = 130;  // SIGKILL convention
    thread_.requestStop();
    paused_.store(false);
    app_.requestAbort();  // VM-level trap; no-op for native/JS apps
}
bool AppHost::shouldExit() const { return thread_.shouldStop(); }
int  AppHost::exitCode()   const { return exitCode_; }
Runtime& AppHost::runtime()      { return rt_; }
const char* AppHost::bundleId()  const { return app_.id(); }

} // namespace nema
