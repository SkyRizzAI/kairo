#include "kairo/sim/bridge.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/log/log_sink.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"
#include "kairo/system/system_info.h"
#include "kairo/system/hardware_registry.h"
#include "kairo/system/capability_registry.h"
#include "kairo/hal/driver.h"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <string>
#include <mutex>

namespace kairo {

// Factory declared in json_stdout_sink.cpp
ILogSink* makeJsonStdoutSink();

// Single serialisation point for ALL stdout JSON (frames from the GuiService
// thread, logs/events from the main thread). Without this lock they interleave.
void simEmit(const std::string& jsonLine) {
    static std::mutex mtx;
    std::lock_guard<std::mutex> lk(mtx);
    std::fprintf(stdout, "%s\n", jsonLine.c_str());
    std::fflush(stdout);
}

static void emitJson(const nlohmann::json& j) {
    simEmit(j.dump());
}

static const char* kindStr(DriverKind k) {
    switch (k) {
        case DriverKind::Battery:   return "Battery";
        case DriverKind::Wifi:      return "Wifi";
        case DriverKind::Bluetooth: return "Bluetooth";
        case DriverKind::Display:   return "Display";
        case DriverKind::Storage:   return "Storage";
        default:                    return "Other";
    }
}

void TelemetryBridge::init(Runtime& rt) {
    rt_ = &rt;

    // Add JSON sink to Logger
    rt.log().addSink(*makeJsonStdoutSink());  // lifetime: until process exit (intentional)

    // When SystemReady fires, send snapshots then the "ready" sentinel
    rt.events().subscribe(events::SystemReady, [this](const Event&) {
        sendSnapshots();
    });

    // Stream all events as JSON (ServiceStarted/Stopped/Failed → type:service)
    rt.events().subscribe("*", [this](const Event& e) {
        std::string evtName(e.name);
        nlohmann::json j;

        if (evtName == events::ServiceStarted ||
            evtName == events::ServiceStopped ||
            evtName == events::ServiceFailed) {
            j["type"] = "service";
            j["ts"]   = rt_->clock().epochMs();
            for (const auto& f : e.payload) j[f.key] = f.value;
            // map event name → state string
            j["state"] = (evtName == events::ServiceStarted) ? "Running"
                       : (evtName == events::ServiceStopped)  ? "Stopped"
                                                               : "Failed";
        } else {
            j["type"] = "event";
            j["ts"]   = rt_->clock().epochMs();
            j["name"] = evtName;
            if (!e.payload.empty()) {
                nlohmann::json payload;
                for (const auto& f : e.payload) payload[f.key] = f.value;
                j["payload"] = payload;
            }
        }
        emitJson(j);
    });
}

void TelemetryBridge::sendSnapshots() {
    uint64_t ts = rt_->clock().epochMs();

    // System info
    const auto& si = rt_->info();
    emitJson({{"type", "system"}, {"ts", ts}, {"info", {
        {"platform", si.platformName}, {"board", si.boardName},
        {"buildVersion", si.buildVersion}, {"firmwareVersion", si.firmwareVersion}
    }}});

    // Hardware
    nlohmann::json hwItems = nlohmann::json::array();
    for (const auto& e : rt_->hardware().list()) {
        hwItems.push_back({{"id", e.id}, {"kind", kindStr(e.kind)}, {"detail", e.detail}});
    }
    emitJson({{"type", "hardware"}, {"ts", ts}, {"items", hwItems}});

    // Capabilities
    nlohmann::json capItems = nlohmann::json::array();
    for (const auto& c : rt_->capabilities().list()) capItems.push_back(c);
    emitJson({{"type", "capability"}, {"ts", ts}, {"items", capItems}});

    // Ready signal
    emitJson({{"type", "ready"}, {"ts", ts}});
}

} // namespace kairo
