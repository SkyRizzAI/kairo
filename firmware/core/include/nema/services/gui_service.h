#pragma once
#include "nema/thread.h"
#include "nema/ui/status_bar.h"
#include "nema/ui/pixelate_server.h"
#include "nema/screens/lock_screen.h"
#include "nema/services/display_power_manager.h"
#include <cstdint>
#include <memory>

namespace nema {

class Runtime;
class Canvas;
struct IDisplayDriver;
struct IDisplayServer;

// GuiService — owns the UI loop on its OWN thread (Nema kernel, core 1).
//
// Furi-style: the GUI is a service running in its own thread that is the SINGLE
// owner of the Canvas + ViewDispatcher. The main loop no longer renders; it only
// ticks background services. Because all UI work (input dispatch, screen
// tick/draw, task completions, status bar) happens here, there is one UI thread
// and no shared-mutable-UI race — provided apps never touch UI state from
// their main-loop onTick (they use TaskRunner + completions, which run here).
//
// Loop each ~15ms:
//   drain InputService → DPM intercepts → ViewDispatcher::handleKey
//   DPM::tick (sleep/lock state machine)
//   refresh status bar (clock/wifi)
//   ViewDispatcher::tick
//   TaskRunner::drainCompletions   (done-callbacks run on the UI thread → safe)
//   render if redraw pending → Canvas (status bar / modal / screen) → flush
//   (render skipped while sleeping; one blank frame flushed on sleep entry)
class GuiService {
public:
    explicit GuiService(Runtime& rt) : rt_(rt), lockScreen_(rt) {}

    void start();   // spawn UI thread
    void stop();    // stop & join

    DisplayPowerManager& dpm() { return dpm_; }

    // FPS API — forwarded to the active display server (PixelateServer owns the
    // rolling 1s flush-count window + the overlay toggle).
    uint16_t fps()         const { return pixelate_ ? pixelate_->fps() : 0; }
    bool     showFps()     const { return pixelate_ && pixelate_->showFps(); }
    void     setShowFps(bool b)  { if (pixelate_) pixelate_->setShowFps(b); }

private:
    static void threadEntry(void* self);
    void        loop();
    void        refreshStatus(uint64_t now);

    Runtime&              rt_;
    nema::Thread          thread_;
    StatusBarData         status_;
    uint64_t              lastStatusMs_ = 0;

    // Pluggable renderer (Plan 43). Default = PixelateServer (the 1-bit canvas
    // UI). server_ points at the active backend; created in start().
    std::unique_ptr<PixelateServer> pixelate_;
    IDisplayServer*                 server_ = nullptr;

    LockScreen            lockScreen_;
    DisplayPowerManager   dpm_;
    IDisplayDriver*       display_ = nullptr;
};

} // namespace nema
