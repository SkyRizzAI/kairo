#pragma once
#include "nema/thread.h"
#include "nema/ui/status_bar.h"
#include "nema/screens/lock_screen.h"
#include "nema/screens/permission_screen.h"
#include <cstdint>

namespace nema {

class Runtime;
struct IDisplayDriver;

// GuiService — owns the UI loop on its OWN thread (Nema kernel, core 1).
//
// Furi-style: the GUI is a service running in its own thread that drives all UI
// work (input dispatch, screen tick/draw, task completions, status bar). There is
// one UI thread and no shared-mutable-UI race — provided apps never touch UI state
// from their main-loop onTick (they use TaskRunner + completions, which run here).
//
// Plan 80: GuiService + the concrete display servers live in the display-server
// lib (aether) and are constructed by the target's main, NOT by Runtime. The loop
// renders whatever server Runtime reports active (rt.displayServer()), applies
// runtime server swaps (rt.applyPendingServer()), and drives the shared sleep/lock
// state machine (rt.dpm()). Core owns neither the loop nor the servers.
//
// Loop each ~33ms:
//   apply pending server swap → drain InputService → DPM intercepts → ViewDispatcher
//   DPM::tick → refresh status bar → ViewDispatcher::tick → TaskRunner completions
//   tick animations → render active server if redraw pending → flush
class GuiService {
public:
    explicit GuiService(Runtime& rt) : rt_(rt), lockScreen_(rt), permScreen_(rt) {}

    void start();   // register fonts, init DPM, spawn UI thread
    void stop();    // stop & join

private:
    static void threadEntry(void* self);
    void        loop();
    void        refreshStatus(uint64_t now);

    Runtime&        rt_;
    nema::Thread    thread_;
    StatusBarData   status_;
    uint64_t        lastStatusMs_ = 0;
    LockScreen       lockScreen_;   // pushed by the DPM on inactivity
    PermissionScreen permScreen_;  // pushed by PermissionService on perm.request()
    IDisplayDriver*  display_ = nullptr;
};

} // namespace nema
