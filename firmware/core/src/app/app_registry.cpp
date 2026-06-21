#include "nema/app/app_registry.h"
#include "nema/app/app.h"
#include "nema/app/app_host_manager.h"
#include "nema/proc/process_host.h"
#include "nema/proc/stream.h"
#include "nema/service.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/display_server.h"
#include "nema/system/capability_registry.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include <cstring>
#include <list>
#include <memory>

namespace {

// Fire-and-forget CLI process: owns stdout/stderr sinks + ProcessHost.
// Stored in a static list; finished entries are pruned on next launchProcess().
struct ProcessEntry {
    nema::LineOutputStream out_;
    nema::LineOutputStream err_;
    nema::ProcessHost      host_;

    ProcessEntry(nema::Runtime& rt, const char* appId,
                 nema::IApp& app, std::vector<std::string> argv)
        : out_([&rt, id = std::string(appId)](const std::string& line) {
              rt.log().info(id.c_str(), line.c_str());
          })
        , err_([&rt, id = std::string(appId)](const std::string& line) {
              rt.log().warn(id.c_str(), line.c_str());
          })
        , host_(rt, app, makeSpec(argv))
    {}

    bool finished() const { return host_.finished(); }
    void start()          { host_.start(); }

private:
    // Called during member initialization — out_ and err_ are already
    // initialized (declared before host_), so &out_/&err_ are valid.
    nema::ProcessSpec makeSpec(std::vector<std::string>& argv) {
        nema::ProcessSpec spec;
        spec.argv    = std::move(argv);
        spec.stdout_ = &out_;
        spec.stderr_ = &err_;
        return spec;
    }
};

static std::list<std::unique_ptr<ProcessEntry>> s_processes;

}  // anonymous namespace

namespace nema {

AppRegistry::AppRegistry(Runtime& rt) : rt_(rt) {}

static bool idEq(const char* a, const char* b) {
    return a && b && std::strcmp(a, b) == 0;
}

void AppRegistry::install(IApp& app, const char* version) {
    AppManifest m{app.id(), app.name(), version, AppKind::BuiltIn, AppType::App};
    m.category = app.category();
    add(m, {&app, nullptr, nullptr});
}

void AppRegistry::installCustom(IApp& app, const char* version) {
    add({app.id(), app.name(), version, AppKind::Custom, AppType::App},
        {&app, nullptr, nullptr});
}

void AppRegistry::installCustom(IApp& app, AppManifest manifest) {
    manifest.kind = AppKind::Custom;
    manifest.type = AppType::App;
    add(manifest, {&app, nullptr, nullptr});
}

void AppRegistry::installScreen(const char* id, const char* name, IScreen& screen,
                                const char* version) {
    add({id, name, version, AppKind::BuiltIn, AppType::App},
        {nullptr, &screen, nullptr});
}

void AppRegistry::installService(IService& svc, const char* id, const char* version) {
    add({id, svc.name(), version, AppKind::BuiltIn, AppType::Service},
        {nullptr, nullptr, &svc});
    rt_.adoptService(&svc);   // boots with the system, or starts now if Running
}

void AppRegistry::add(const AppManifest& m, Target t) {
    const char* kindStr = (m.kind == AppKind::Custom) ? "custom" : "builtin";

    // Re-install with a known id → replace the entry in place (e.g. an OTA
    // custom app rebuilt, or a built-in swapped during bring-up).
    for (size_t i = 0; i < manifests_.size(); i++) {
        if (idEq(manifests_[i].id, m.id)) {
            // Replacing a service entry → stop the old instance first.
            if (targets_[i].service && targets_[i].service != t.service)
                rt_.dropService(targets_[i].service);
            targets_[i]   = t;
            manifests_[i] = m;
            rt_.log().info("AppRegistry", std::string("reinstalled: ") + m.name,
                {{"id", m.id}, {"kind", kindStr}});
            return;
        }
    }

    targets_.push_back(t);
    manifests_.push_back(m);
    const char* typeStr = (m.type == AppType::Service) ? "service" : "app";
    rt_.log().info("AppRegistry", std::string("installed: ") + m.name,
        {{"id", m.id}, {"version", m.version}, {"kind", kindStr}, {"type", typeStr}});
    rt_.events().publish({events::AppInstalled, {{"id", m.id}, {"name", m.name}}});
}

void AppRegistry::uninstall(const char* id) {
    for (size_t i = 0; i < manifests_.size(); i++) {
        if (idEq(manifests_[i].id, id)) {
            // Service entries: stop + untrack before forgetting them, or the
            // ServiceManager would keep ticking a ghost.
            if (targets_[i].service) rt_.dropService(targets_[i].service);
            rt_.events().publish({events::AppRemoved, {{"id", id}}});
            targets_.erase(targets_.begin() + i);
            manifests_.erase(manifests_.begin() + i);
            rt_.log().info("AppRegistry", std::string("uninstalled: ") + id);
            return;
        }
    }
}

bool AppRegistry::isInstalled(const char* id) const {
    for (const auto& m : manifests_) if (idEq(m.id, id)) return true;
    return false;
}

bool AppRegistry::launch(const char* id) {
    for (size_t i = 0; i < manifests_.size(); i++) {
        if (!idEq(manifests_[i].id, id)) continue;
        const AppManifest& m = manifests_[i];
        const Target& t = targets_[i];

        // Plan 51 — negotiate display server before launching the app.
        // Check requiredCaps() eligibility first; deny the launch if the
        // board lacks a capability the target server needs (e.g. "display").
        if (m.displayServer && m.displayServer[0]) {
            if (IDisplayServer* srv = rt_.findDisplayServer(m.displayServer)) {
                if (const char* const* caps = srv->requiredCaps()) {
                    for (const char* const* c = caps; *c; c++) {
                        if (!rt_.capabilities().has(*c)) {
                            rt_.log().warn("AppRegistry",
                                std::string("launch: server '") + m.displayServer
                                + "' requires cap '" + *c + "' — unavailable for " + id);
                            return false;
                        }
                    }
                }
            }
            if (!rt_.switchDisplayServer(m.displayServer)) {
                rt_.log().warn("AppRegistry", std::string("launch: server '")
                    + m.displayServer + "' unavailable for " + id);
            }
        }

        // CLI-only apps don't need a surface: spawn headless with ProcessContext.
        if (m.mode == AppMode::Cli && t.app) {
            return launchProcess(id);
        }

        if (t.app)    { rt_.appHost().launch(*t.app); return true; }  // own thread (UI)
        if (t.screen) { rt_.view().push(*t.screen);   return true; }  // UI-thread view
        // Services aren't launchable — they're already running in background.
        rt_.log().warn("AppRegistry", std::string("launch: ") + id + " is a service");
        return false;
    }
    rt_.log().warn("AppRegistry", std::string("launch: unknown app ") + (id ? id : "(null)"));
    return false;
}

bool AppRegistry::launchProcess(const char* id, std::vector<std::string> argv) {
    // Prune finished process entries before adding new ones.
    s_processes.remove_if([](const auto& e) { return e->finished(); });

    for (size_t i = 0; i < manifests_.size(); i++) {
        if (!idEq(manifests_[i].id, id)) continue;
        IApp* app = targets_[i].app;
        if (!app) {
            rt_.log().warn("AppRegistry",
                std::string("launchProcess: ") + id + " has no IApp");
            return false;
        }
        if (argv.empty()) argv.push_back(id);

        auto entry = std::make_unique<ProcessEntry>(rt_, id, *app, std::move(argv));
        entry->start();
        s_processes.push_back(std::move(entry));

        rt_.log().info("AppRegistry", std::string("launched process: ") + id);
        return true;
    }
    rt_.log().warn("AppRegistry",
        std::string("launchProcess: unknown app ") + (id ? id : "(null)"));
    return false;
}

IApp* AppRegistry::getApp(const char* id) const {
    for (size_t i = 0; i < manifests_.size(); i++) {
        if (idEq(manifests_[i].id, id)) return targets_[i].app;
    }
    return nullptr;
}

} // namespace nema
