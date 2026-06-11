#include "kairo/app/app_host.h"
#include "kairo/app/app.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/hal/buffer_display.h"
#include "kairo/hal/display.h"
#include "kairo/service/service_container.h"
#include <cstdlib>
#include <cstring>

#ifdef ESP_PLATFORM
  #include <esp_heap_caps.h>
#endif

namespace kairo {

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

AppHost::AppHost(Runtime& rt, IApp& app) : rt_(rt), app_(app) {}

AppHost::~AppHost() {
    delete appCanvas_;
    delete bufDisplay_;
    freeBuf(drawBuf_);
    freeBuf(readyBuf_);
}

// ── GUI thread ────────────────────────────────────────────────────────────

void AppHost::enter() {
    if (started_) {
        // Re-enter. Either revealed by a child screen pop (no-op), or resumed
        // from pause (Plan 22) — clear the pause flag so the parked app thread
        // wakes from waitInput() and repaints.
        if (paused_.load()) { paused_.store(false); rt_.view().requestRedraw(); }
        return;
    }
    w_    = rt_.canvas().width();
    h_    = rt_.canvas().height();
    size_ = (size_t)w_ * h_;
    drawBuf_  = allocBuf(size_);              // written by app thread (PSRAM ok)
    readyBuf_ = allocBuf(size_, true);        // read every frame by blit → internal SRAM
    std::memset(drawBuf_,  0, size_);
    std::memset(readyBuf_, 0, size_);
    bufDisplay_ = new BufferDisplay(drawBuf_, w_, h_);
    appCanvas_  = new Canvas(*bufDisplay_);
    display_    = rt_.container().resolve<IDisplayDriver>();  // for fast fullscreen path

    started_  = true;
    finished_ = false;
    // App thread on core 0 (off the GUI/core-1). May block freely.
    // Stack size comes from the app: JS apps need more (QuickJS recurses deeply).
    thread_.start({"nema_app", app_.stackBytes(), 5, 0}, &AppHost::threadEntry, this);
    rt_.view().requestRedraw();
}

void AppHost::update(Key key) {
    InputEvent ie;
    ie.kind = InputEvent::Kind::Key;
    ie.key  = key;
    ie.type = InputEvent::Type::Press;
    mailbox_.send(ie);
}

void AppHost::onPointer(const input::PointerEvent& e) {
    InputEvent ie;
    ie.kind   = InputEvent::Kind::Pointer;
    ie.type   = InputEvent::Type::Press;
    ie.pphase = e.phase;
    ie.px = e.x; ie.py = e.y;
    mailbox_.send(ie);   // delivered to app thread via waitInput()
}

ScreenMode AppHost::mode() const {
    return app_.fullscreen() ? ScreenMode::Fullscreen : ScreenMode::Normal;
}

void AppHost::draw(Canvas& c) {
    std::lock_guard<std::mutex> lk(frameMtx_);
    if (!hasFrame_) return;

    // Fullscreen fast path: push the whole frame straight to the panel in one
    // pass (skips 76,800 per-pixel drawPixel calls). Only valid when the app
    // buffer is at physical dimensions (scale == 1.0); at scale > 1.0, w_/h_
    // are logical (smaller) and flushBuffer would silently no-op.
    if (app_.fullscreen() && display_ &&
        w_ == display_->width() && h_ == display_->height()) {
        display_->flushBuffer(readyBuf_, w_, h_);
        return;
    }

    // Scaled fullscreen or normal mode: blit via canvas drawPixel.
    // Fullscreen apps own the whole canvas; clear first so physical-pixel gaps
    // left by non-integer scale don't show stale content at the edges.
    if (app_.fullscreen()) c.clear(false);
    uint16_t top = app_.fullscreen() ? 0 : (uint16_t)(ui::SEP1_Y + 2);
    for (uint16_t y = top; y < h_; y++)
        for (uint16_t x = 0; x < w_; x++)
            c.drawPixel(x, y, readyBuf_[(size_t)y * w_ + x] != 0);
}

bool AppHost::suppressCanvasFlush() const {
    // Suppress only when we actually took the flushBuffer fast path (physical
    // dimensions match). At scale > 1.0 we fall through to the canvas blit path
    // and GuiService must flush normally.
    return app_.fullscreen() && display_ != nullptr &&
           w_ == display_->width() && h_ == display_->height();
}

void AppHost::tick(uint64_t) {
    if (started_ && !finished_ && !thread_.running()) {
        finished_ = true;
        thread_.join();
        rt_.view().pop();    // app exited → return to previous screen
    }
}

// ── App thread ──────────────────────────────────────────────────────────────

void AppHost::threadEntry(void* self) {
    auto* h = static_cast<AppHost*>(self);
    h->app_.run(*h);   // blocks here for the app's whole lifetime
}

Canvas& AppHost::canvas() { return *appCanvas_; }

void AppHost::present() {
    {
        std::lock_guard<std::mutex> lk(frameMtx_);
        std::memcpy(readyBuf_, drawBuf_, size_);
        hasFrame_ = true;
    }
    rt_.view().requestRedraw();   // ask GUI thread to re-blit
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

void AppHost::requestExit()      { thread_.requestStop(); paused_.store(false); }
bool AppHost::shouldExit() const { return thread_.shouldStop(); }
Runtime& AppHost::runtime()      { return rt_; }

} // namespace kairo
