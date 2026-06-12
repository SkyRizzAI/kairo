#pragma once
#include "nema/ui/screen.h"
#include "nema/app/app_context.h"
#include "nema/thread.h"
#include "nema/message_queue.h"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>

namespace nema {

class Runtime;
struct IApp;
class Canvas;
class BufferDisplay;
struct IDisplayDriver;

// AppHost — bridges an IApp (running on its own thread) into the ViewDispatcher.
//
// To the GUI thread it is just an IScreen (Fullscreen): draw() blits the latest
// frame the app presented; update() forwards the key into the app's input
// mailbox; tick() detects when the app thread has exited and pops itself.
//
// To the app it is the AppContext: canvas() draws into an in-RAM buffer,
// present() publishes that buffer, nextInput()/waitInput() read the mailbox.
//
// Shared state between the two threads is ONLY the pixel buffer (guarded by
// frameMtx_) and the input queue (already thread-safe). No shared model → no race.
class AppHost : public IScreen, public AppContext {
public:
    AppHost(Runtime& rt, IApp& app);
    ~AppHost() override;

    // IScreen (GUI thread). mode follows the app: fullscreen apps take the whole
    // screen; otherwise Normal so GuiService draws the status bar above the app.
    ScreenMode mode() const override;
    void enter()         override;   // spawn app thread (or resume if already running)

    // Pause/resume (Plan 22). Paused: the app thread parks inside waitInput()
    // (CPU ~0, stack+state preserved); resume = enter() again (re-push) clears it.
    void        setPaused(bool v) { paused_.store(v, std::memory_order_release); }
    bool        isPaused() const  { return paused_.load(std::memory_order_acquire); }
    const char* appName() const;
    void update(Key key) override;   // forward key → mailbox
    void onPointer(const input::PointerEvent& e) override;  // forward touch → mailbox
    void draw(Canvas& c) override;   // blit latest app frame
    bool suppressCanvasFlush() const override;  // fullscreen → we flush directly
    void tick(uint64_t nowMs) override;  // pop self when app thread finishes

    // AppContext (app thread)
    Canvas&  canvas()  override;
    void     present() override;
    bool     nextInput(InputEvent& out) override;
    bool     waitInput(InputEvent& out, uint32_t timeoutMs) override;
    void     requestExit() override;
    bool     shouldExit() const override;
    Runtime& runtime() override;

private:
    static void threadEntry(void* self);

    Runtime& rt_;
    IApp&    app_;

    uint16_t w_ = 0, h_ = 0;
    size_t   size_ = 0;
    uint8_t* drawBuf_  = nullptr;  // app renders here (app thread only)
    uint8_t* readyBuf_ = nullptr;  // latest presented frame (shared)
    bool     hasFrame_ = false;
    std::mutex frameMtx_;
    // Frame sequence: bumped by present() (app thread), compared in tick() (GUI
    // thread) so the latest presented frame is ALWAYS eventually drawn even if a
    // single requestRedraw signal is lost/mistimed across the thread boundary.
    std::atomic<uint32_t> frameSeq_{0};
    uint32_t              drawnSeq_ = 0;   // GUI-thread only

    BufferDisplay*           bufDisplay_ = nullptr;  // wraps drawBuf_
    Canvas*                  appCanvas_  = nullptr;  // app draws via this
    IDisplayDriver*          display_    = nullptr;  // for fast fullscreen flushBuffer
    nema::MessageQueue<InputEvent> mailbox_{16};
    nema::Thread             thread_;
    bool                     started_  = false;
    bool                     finished_ = false;
    std::atomic<bool>        paused_{false};   // Plan 22: app thread parked
    uint32_t                 dbgPresent_ = 0;  // diagnostic: count first presents
    uint32_t                 dbgDraw_    = 0;  // diagnostic: count first draws
};

} // namespace nema
