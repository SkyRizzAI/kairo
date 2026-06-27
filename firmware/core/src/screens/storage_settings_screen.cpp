// Plan 83 Fase 5 — StorageSettingsScreen: volume overview only.
// Per-app detail (permissions + move + uninstall) moved to AppDetailScreen (Plan 87 Fase 7).
// Plan 89: async load via TaskRunner (Fase 3) + SD card info + eject (Fase 4).
#include "nema/screens/storage_settings_screen.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/ui/view_dispatcher.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

StorageSettingsScreen::StorageSettingsScreen(Runtime& rt)
    : ComponentScreen(rt, 160), confirm_(rt) {}

void StorageSettingsScreen::onResume() {
    vals_.clear();
    cached_.ready = false;
    dirty_ = true;  // show "Loading…" immediately
    rt_.view().requestRedraw();

    auto* svc = rt_.container().resolve<StorageService>();
    if (!svc) return;

    // Kick an async task so the VFS scan doesn't block the UI thread.
    // Worker writes to cached_; done callback sets ready = true and redraws.
    rt_.tasks().submit(
        [svc, this]() {
            cached_.intVol = svc->internalVolume();
            cached_.sdInfo = svc->sdCardInfo();
            if (cached_.sdInfo.mounted) cached_.extVol = svc->externalVolume();
        },
        [this]() {
            cached_.ready = true;
            vals_.clear();
            dirty_ = true;
            rt_.view().requestRedraw();
        }
    );
}

std::string StorageSettingsScreen::fmtBytes(size_t bytes) {
    char buf[24];
    if (bytes >= 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%zu MB", bytes / (1024 * 1024));
    else if (bytes >= 1024)
        std::snprintf(buf, sizeof(buf), "%zu KB", bytes / 1024);
    else
        std::snprintf(buf, sizeof(buf), "%zu B", bytes);
    return buf;
}

static std::string fmtBytes64(uint64_t bytes) {
    char buf[24];
    if (bytes >= (uint64_t)1024 * 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%llu GB", (unsigned long long)(bytes / (1024ULL * 1024 * 1024)));
    else if (bytes >= 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%llu MB", (unsigned long long)(bytes / (1024 * 1024)));
    else if (bytes >= 1024)
        std::snprintf(buf, sizeof(buf), "%llu KB", (unsigned long long)(bytes / 1024));
    else
        std::snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    return buf;
}

#define S(u) static_cast<StorageSettingsScreen*>(u)
aether::ui::UiNode* StorageSettingsScreen::build(NodeArena& a, Runtime& /*rt*/) {
    MenuBuilder m(a, scroll_, this);

    // vals_ owns all formatted value strings — ListEntry holds const char* into them.
    vals_.clear();
    auto pushVal = [&](const std::string& s) -> const char* {
        vals_.push_back(s);
        return vals_.back().c_str();
    };

    m.section("Storage");

    if (!cached_.ready) {
        m.info("Loading…", nullptr);
        return m.build();
    }

    // ── Internal flash ────────────────────────────────────────────────────────
    {
        const auto& v = cached_.intVol;
        std::string val = v.totalBytes > 0
            ? fmtBytes(v.usedBytes) + " / " + fmtBytes(v.totalBytes)
            : fmtBytes(v.usedBytes) + " used";
        intCapPct_ = (v.totalBytes > 0)
            ? (int)((uint64_t)v.usedBytes * 100 / v.totalBytes) : 0;
        m.info("Internal Flash", pushVal(val));
        m.progress(intCapPct_);
    }

    // ── SD card ───────────────────────────────────────────────────────────────
    const auto& sd = cached_.sdInfo;
    if (!sd.mounted) {
        m.info("SD Card", "Not mounted");
    } else {
        // Show capacity if the backend reported it.
        std::string sdVal;
        if (sd.totalBytes > 0) {
            uint64_t used = sd.totalBytes > sd.freeBytes
                            ? sd.totalBytes - sd.freeBytes : 0;
            sdVal = fmtBytes64(used) + " / " + fmtBytes64(sd.totalBytes);
            sdCapPct_ = (int)(used * 100 / sd.totalBytes);
        } else {
            sdVal = fmtBytes(cached_.extVol.usedBytes) + " used";
            sdCapPct_ = 0;
        }
        m.info("SD Card", pushVal(sdVal));
        m.progress(sdCapPct_);
        // Eject action row — gated behind a confirmation modal.
        m.nav("Eject SD Card", [](void* u){
            auto* self = S(u);
            self->confirm_.setup("Eject SD Card", "Safely eject the\nSD card?", "Eject",
                                 doEject, self, /*danger=*/false);
            self->rt_.view().push(self->confirm_);
        });
    }

    return m.build();
}
#undef S

void StorageSettingsScreen::doEject(void* u) {
    auto* self = static_cast<StorageSettingsScreen*>(u);
    self->rt_.view().goBack();   // pop the modal
    auto* svc = self->rt_.container().resolve<StorageService>();
    if (!svc) return;
    self->runBusy("Ejecting…",
                  [svc] { svc->ejectSd(); },
                  [self] { self->onResume(); });   // fresh async load reflects ejected state
}

} // namespace nema
