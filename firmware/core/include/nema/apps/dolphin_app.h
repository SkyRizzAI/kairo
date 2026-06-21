#pragma once
// Plan 71 — Dolphin animation showcase (custom app, dynamic VFS loading).
// Plan 82 Phase 5 — loads from .panim files instead of seeding .bm files.
#include "nema/app/component_app.h"
#include "nema/ui/asset_loader.h"
#include <vector>
#include <string>
#include <memory>

namespace nema {

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
    aether::ui::UiNode* build(aether::ui::NodeArena& a, AppContext& ctx) override { (void)a; (void)ctx; return nullptr; }

private:
    void drawLoading(AppContext& ctx, int done, int total);

    struct Entry {
        std::string                             name;
        uint16_t w = 0, h = 0;
        uint8_t fps = 0;
        std::unique_ptr<asset::PanimAsset>      anim;
        std::unique_ptr<anim::AnimationPlayer>  player;
    };
    std::vector<Entry> entries_;

    int      animIdx_ = 0;
    bool     paused_  = false;
    bool     dirty_   = true;
};

} // namespace nema
