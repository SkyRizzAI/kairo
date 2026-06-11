#include "kairo/app/app_registry.h"
#include "kairo/app/app.h"
#include "kairo/app/app_host_manager.h"
#include "kairo/service.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"
#include <cstring>

namespace kairo {

AppRegistry::AppRegistry(Runtime& rt) : rt_(rt) {}

static bool idEq(const char* a, const char* b) {
    return a && b && std::strcmp(a, b) == 0;
}

void AppRegistry::install(IApp& app, const char* version) {
    add({app.id(), app.name(), version, AppKind::BuiltIn, AppType::App},
        {&app, nullptr, nullptr});
}

void AppRegistry::installCustom(IApp& app, const char* version) {
    add({app.id(), app.name(), version, AppKind::Custom, AppType::App},
        {&app, nullptr, nullptr});
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
        const Target& t = targets_[i];
        if (t.app)         { rt_.appHost().launch(*t.app); return true; }  // own thread
        if (t.screen)      { rt_.view().push(*t.screen);   return true; }  // UI-thread view
        // Services aren't launchable — they're already running in background.
        rt_.log().warn("AppRegistry", std::string("launch: ") + id + " is a service");
        return false;
    }
    rt_.log().warn("AppRegistry", std::string("launch: unknown app ") + (id ? id : "(null)"));
    return false;
}

} // namespace kairo
