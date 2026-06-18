#pragma once
#include "nema/ui/animation_player.h"
#include <vector>

namespace nema::anim {

// Plan 70: Global animation tick manager. Called from GuiService::loop() each
// frame. All registered AnimationPlayer instances are ticked; if any frame
// advances, the caller should request a redraw.
//
// Registration is not thread-safe — add/remove only from the GUI thread.
class AnimationManager {
public:
    static AnimationManager& instance();

    void registerPlayer(AnimationPlayer& p);
    void unregisterPlayer(AnimationPlayer& p);

    // Tick all registered players. Returns true if ANY player advanced its frame
    // (the caller should requestRedraw).
    bool tickAll(uint32_t nowMs);

    size_t activeCount() const;

private:
    AnimationManager() = default;
    std::vector<AnimationPlayer*> players_;
};

} // namespace nema::anim
