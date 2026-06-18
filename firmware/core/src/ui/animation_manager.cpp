#include "nema/ui/animation_manager.h"

namespace nema::anim {

AnimationManager& AnimationManager::instance() {
    static AnimationManager mgr;
    return mgr;
}

void AnimationManager::registerPlayer(AnimationPlayer& p) {
    players_.push_back(&p);
}

void AnimationManager::unregisterPlayer(AnimationPlayer& p) {
    for (auto it = players_.begin(); it != players_.end(); ++it) {
        if (*it == &p) { players_.erase(it); return; }
    }
}

bool AnimationManager::tickAll(uint32_t nowMs) {
    bool any = false;
    for (auto* p : players_) {
        if (p->tick(nowMs)) any = true;
    }
    return any;
}

size_t AnimationManager::activeCount() const {
    return players_.size();
}

} // namespace nema::anim
