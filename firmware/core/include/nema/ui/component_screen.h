#pragma once
#include "nema/ui/screen.h"
#include "nema/ui/widgets.h"
#include "nema/ui/component_runtime.h"

namespace nema {

class Runtime;

// ComponentScreen — base for system screens built with the retained-mode
// component tree (Plan 30). A subclass implements build() and gets, for free and
// identical to ComponentApp: flex layout, focus ring (focus-visible), tap, touch
// drag-scroll and flick momentum — all via ComponentRuntime.
//
// Lifecycle inside the GuiService loop: input (onAction/onPointer) runs against
// the tree from the previous draw(); draw() rebuilds the tree and renders.
class ComponentScreen : public IScreen {
public:
    explicit ComponentScreen(Runtime& rt, size_t arenaCap = 256)
        : rt_(rt), arena_(arenaCap) {}

    // Subclass entry point — return this frame's node tree (built from `arena`).
    virtual aether::ui::UiNode* build(aether::ui::NodeArena& arena, Runtime& rt) = 0;

    // Plan 70: Optional Back hook. Return true to consume (prevent pop).
    // Deprecated: override onBackPressed() instead (from IScreen).
    virtual bool onBack() { return false; }

    // IScreen
    void enter() override;    // resets modality → focus ring re-appears on wake
    void onResume() override; // Plan 70: called when screen becomes active
    void onAction(input::Action a) override;
    void onPointer(const input::PointerEvent& e) override;
    void draw(Canvas& c) override;
    void tick(uint64_t nowMs) override;
    ScreenMode mode() const override { return fullscreen() ? ScreenMode::Fullscreen
                                                           : ScreenMode::Normal; }

protected:
    // Override to paint over the whole canvas (no status bar strip); the tree is
    // laid out from y=0 with full height. Used by e.g. the lock screen.
    virtual bool fullscreen() const { return false; }

    void requestRedraw();   // marks the view dirty (defined in .cpp; needs Runtime)

    Runtime&           rt_;
    aether::ui::NodeArena      arena_;
    aether::ui::ComponentState state_;
    aether::ui::UiNode*        root_        = nullptr;
    bool               dirty_       = true;   // Plan 70: only rebuild tree when model changed
    uint64_t           lastMarqueeMs_ = 0;    // rate-limiter for continuous marquee redraws
};

} // namespace nema
