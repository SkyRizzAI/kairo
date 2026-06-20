#pragma once
#include "nema/app/component_app.h"
#include "nema/ui/asset_loader.h"
#include <vector>
#include <string>
#include <memory>   // std::unique_ptr (Entry::player) — needed on the ESP-IDF gcc toolchain

namespace nema {

// Plan 71 — Dolphin animation showcase (custom app, dynamic VFS loading).
// Seeds VFS with dolphin .bm files at launch, then loads all animations
// via AnimAsset::load(). No compiled-in Animation dependencies.
// Proves: custom apps can dynamically load frame animations from filesystem.
class DolphinApp : public ComponentApp {
public:
    const char* id()   const override { return "com.palanu.dolphin"; }
    const char* name() const override { return "Dolphin Showcase"; }

protected:
    bool       capturesInput() const override { return true; }
    bool       drawRaw(Canvas& c, AppContext& ctx) override;
    bool       onKey(Key k, AppContext& ctx) override;
    void       onStart(AppContext& ctx) override;
    uint32_t   tickIntervalMs() const override { return 33; }
    bool       onTick(AppContext& ctx) override;
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override { (void)a; (void)ctx; return nullptr; }

private:
    void seedDolphinAssets(AppContext& ctx);
    // Draw a "Loading n/total" frame + progress bar and present it, so the first
    // launch shows feedback instead of a black screen while assets seed/load.
    void drawLoading(AppContext& ctx, int done, int total);

    struct Entry {
        std::string              name;
        asset::AnimAsset         anim;
        std::unique_ptr<anim::AnimationPlayer> player;
    };
    std::vector<Entry> entries_;

    int      animIdx_ = 0;
    bool     paused_  = false;
    uint32_t tickCnt_ = 0;
    bool     dirty_   = true;
};

} // namespace nema
