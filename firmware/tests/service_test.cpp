// Host test for the dynamic service lifecycle (app-installed services).
// Covers the AppRegistry::installService contract at the ServiceManager level:
//  - a service registered BEFORE startAll boots with the system ("on boot"),
//  - a service adopted AFTER startAll starts via startOne and ticks from the
//    next frame ("installed at runtime by an app"),
//  - stopOne/removeService cleanly retire it,
//  - startOne is idempotent, addService is idempotent.
#include "kairo/service/service_manager.h"
#include "kairo/service/service_container.h"
#include "kairo/service.h"
#include "kairo/event/event_bus.h"
#include "kairo/log/logger.h"
#include "kairo/clock.h"
#include <cassert>
#include <cstdio>

using namespace kairo;

namespace {

struct FakeClock : IClock {
    uint64_t now = 0;
    uint64_t millis()  override { return now; }
    uint64_t epochMs() override { return now; }
};

struct ProbeService : IService {
    const char* name_;
    int starts = 0, stops = 0, ticks = 0;
    explicit ProbeService(const char* n) : name_(n) {}
    const char* name() const override { return name_; }
    void start() override { starts++; }
    void stop()  override { stops++; }
    void tick(uint64_t) override { ticks++; }
};

} // namespace

int main() {
    FakeClock clock;
    Logger    log(clock);
    EventBus  bus;
    ServiceContainer container;

    // ── 1. "On boot": registered before startAll → started + ticked ──
    ProbeService boot("BootSvc");
    container.addService(&boot);

    ServiceManager mgr(container, log, bus);
    mgr.startAll();
    assert(boot.starts == 1);
    assert(mgr.stateOf(&boot) == ServiceState::Running);

    mgr.tickAll(10);
    assert(boot.ticks == 1);

    // ── 2. "Installed at runtime": adopted after startAll ──
    ProbeService late("LateSvc");
    container.addService(&late);

    // Without startOne it would sit in Created and never tick (the old plugin
    // registerService bug). The dynamic path starts it explicitly:
    mgr.startOne(&late);
    assert(late.starts == 1);
    assert(mgr.stateOf(&late) == ServiceState::Running);

    mgr.tickAll(20);
    assert(boot.ticks == 2);
    assert(late.ticks == 1);

    // startOne is idempotent — no double start on an already-Running service.
    mgr.startOne(&late);
    assert(late.starts == 1);

    // addService is idempotent — re-adding doesn't duplicate ticks.
    container.addService(&late);
    mgr.tickAll(30);
    assert(late.ticks == 2);

    // ── 3. Retire: stopOne + removeService ──
    mgr.stopOne(&late);
    assert(late.stops == 1);
    assert(mgr.stateOf(&late) == ServiceState::Stopped);
    container.removeService(&late);

    mgr.tickAll(40);
    assert(late.ticks == 2);   // no longer ticked
    assert(boot.ticks == 4);   // others unaffected (ticked at 10/20/30/40)

    // stopOne on a non-running service is a no-op.
    mgr.stopOne(&late);
    assert(late.stops == 1);

    // ── 4. Shutdown still works for the rest ──
    mgr.stopAll();
    assert(boot.stops == 1);
    assert(mgr.stateOf(&boot) == ServiceState::Stopped);

    std::printf("service_test: all assertions passed\n");
    return 0;
}
