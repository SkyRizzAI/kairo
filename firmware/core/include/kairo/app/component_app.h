#pragma once
#include "kairo/app/app.h"
#include "kairo/ui/node.h"
#include "kairo/ui/widgets.h"
#include "kairo/ui/key.h"

namespace kairo {

class AppContext;
class Canvas;

// ComponentApp — base for apps written declaratively with the component system.
//
// Subclass implements build() to return a UiNode tree from the arena + its own
// state. The base runs the loop: build → layout → render → present; input drives
// focus navigation (4 arrows move focus, Select fires the focused Pressable's
// onPress) and Cancel bubbles to onKey()/exit. Re-render on any handled key.
//
// onPress callbacks mutate subclass state (pass `this` as userdata); the next
// build() reflects the new state. No manual coordinate math, no cursor handling.
class ComponentApp : public IApp {
public:
    void run(AppContext& ctx) override;

protected:
    // Called once before the loop starts (e.g. kick off an initial fetch).
    virtual void onStart(AppContext& ctx) { (void)ctx; }

    // Build the tree for the current state. Called every render; use the arena.
    virtual ui::UiNode* build(ui::NodeArena& arena, AppContext& ctx) = 0;

    // Handle a non-navigation key (incl. Cancel). Return true if consumed
    // (triggers re-render); false lets the base act (Cancel → exit).
    virtual bool onKey(Key k, AppContext& ctx) { (void)k; (void)ctx; return false; }

    // Periodic wake interval in ms for live apps (clock/stopwatch). 0 = redraw
    // only on input (default — best for e-ink, no needless refreshes).
    virtual uint32_t tickIntervalMs() const { return 0; }

    // Called on each periodic wake (no input). Return true if the displayed
    // state changed and a re-render is needed (preserves the skip-repaint
    // optimisation — only present when something actually changed).
    virtual bool onTick(AppContext& ctx) { (void)ctx; return false; }

    // Escape hatch for raw-input states (e.g. a virtual keyboard). When
    // capturesInput() is true, ALL keys route straight to onKey() (focus nav
    // bypassed). When drawRaw() returns true it has painted the whole frame
    // itself, so the base skips the component tree. Together these let a
    // component app host a custom-drawn screen without leaving the model.
    virtual bool capturesInput() const { return false; }
    virtual bool drawRaw(Canvas& c, AppContext& ctx) { (void)c; (void)ctx; return false; }

    // Arena capacity (override if a screen needs a bigger tree).
    virtual size_t arenaCapacity() const { return 256; }
};

} // namespace kairo
