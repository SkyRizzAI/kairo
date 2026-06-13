// Host unit test for the two-axis CapabilityRegistry (Plan 42).
#include "nema/system/capability_registry.h"
#include "nema/system/capabilities.h"
#include "nema/event/event_bus.h"
#include <cstdio>
#include <string>

using namespace nema;

static int fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); fail++; } \
                         else       std::printf("  ok:   %s\n", m); } while (0)

int main() {
    std::printf("== CapabilityRegistry (two-axis) tests ==\n");

    CapabilityRegistry caps;

    // --- Axis 1: static, idempotent, ordered ---
    caps.add(caps::Display);
    caps.add(caps::Camera);
    caps.add(caps::Display);                 // duplicate
    CHECK(caps.has(caps::Display), "has() true after add");
    CHECK(!caps.has(caps::NetWifi), "has() false for un-added cap");
    CHECK(caps.list().size() == 2, "duplicate add is deduped (list size 2)");
    CHECK(caps.list()[0] == std::string(caps::Display), "list keeps insertion order");

    // --- Axis 2: default-available for static cap with no liveness report ---
    CHECK(caps.available(caps::Display), "static cap defaults to Available");
    CHECK(caps.stateOf(caps::Display) == ResourceState::Available, "stateOf default Available");
    CHECK(caps.stateOf(caps::NetWifi) == ResourceState::Absent, "unknown cap stateOf Absent");
    CHECK(!caps.available(caps::NetWifi), "unknown cap not available");

    // --- Axis 2: explicit liveness wins ---
    caps.setState(caps::Camera, ResourceState::Fault);
    CHECK(caps.has(caps::Camera), "has() still true when faulted (static unchanged)");
    CHECK(!caps.available(caps::Camera), "faulted cap not available");
    CHECK(caps.stateOf(caps::Camera) == ResourceState::Fault, "stateOf reports Fault");
    caps.setState(caps::Camera, ResourceState::Available);
    CHECK(caps.available(caps::Camera), "cap available again after recover");

    // setState on a cap that was never add()ed: liveness recorded, but has() false
    caps.setState(caps::Storage, ResourceState::Available);
    CHECK(!caps.has(caps::Storage), "setState does not imply static has()");
    CHECK(!caps.available(caps::Storage), "available() requires static has()");

    // --- Event emission via bus ---
    EventBus bus;
    caps.setBus(&bus);
    int events = 0;
    std::string lastResource, lastState;
    bus.subscribe(events::ResourceChanged, [&](const Event& e) {
        events++;
        for (const auto& f : e.payload) {
            if (std::string(f.key) == "resource") lastResource = f.value;
            if (std::string(f.key) == "state")    lastState    = f.value;
        }
    });
    caps.setState(caps::Display, ResourceState::Available);  // change → 1 event
    CHECK(events == 1, "setState publishes ResourceChanged");
    CHECK(lastResource == std::string(caps::Display), "event carries resource id");
    CHECK(lastState == "available", "event carries state string");
    caps.setState(caps::Display, ResourceState::Available);  // no change → no event
    CHECK(events == 1, "no-op setState publishes nothing");

    std::printf(fail == 0 ? "== ALL PASS ==\n" : "== FAILURES ==\n");
    return fail == 0 ? 0 : 1;
}
