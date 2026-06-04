#pragma once
#include "kairo/nema/thread.h"
#include "kairo/ui/status_bar.h"
#include "kairo/screens/lock_screen.h"
#include "kairo/services/display_power_manager.h"
#include <cstdint>

namespace kairo {

class Runtime;
class Canvas;
struct IDisplayDriver;

// GuiService — owns the UI loop on its OWN thread (Nema kernel, core 1).
//
// Furi-style: the GUI is a service running in its own thread that is the SINGLE
// owner of the Canvas + ViewDispatcher. The main loop no longer renders; it only
// ticks background services/plugins. Because all UI work (input dispatch, screen
// tick/draw, task completions, status bar) happens here, there is one UI thread
// and no shared-mutable-UI race — provided plugins never touch UI state from
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
    explicit GuiService(Runtime& rt) : rt_(rt) {}

    void start();   // spawn UI thread
    void stop();    // stop & join

    DisplayPowerManager& dpm() { return dpm_; }

private:
    static void threadEntry(void* self);
    void        loop();
    void        renderOnce(Canvas& c);
    void        refreshStatus(uint64_t now);

    Runtime&              rt_;
    nema::Thread          thread_;
    StatusBarData         status_;
    uint64_t              lastStatusMs_ = 0;

    LockScreen            lockScreen_;
    DisplayPowerManager   dpm_;
    IDisplayDriver*       display_ = nullptr;
};

} // namespace kairo
