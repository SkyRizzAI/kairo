#include "nema/ui/animation_player.h"

namespace nema::anim {

AnimationPlayer::AnimationPlayer(const Animation& def)
    : def_(def) {}

const uint8_t* AnimationPlayer::currentFrameData() const {
    return currentDef().bitmap;
}

void AnimationPlayer::start() {
    if (def_.frameCount == 0) return;
    playing_   = true;
    finished_  = false;
    lastTickMs_ = 0;   // first tick will advance immediately
}

void AnimationPlayer::stop() {
    playing_  = false;
    finished_ = false;
    frame_    = 0;
}

void AnimationPlayer::pause() {
    playing_ = false;
}

bool AnimationPlayer::tick(uint32_t nowMs) {
    if (!playing_ || def_.frameCount == 0 || def_.frameRate == 0) return false;

    // Convert frame rate to ms per frame
    uint32_t msPerFrame = 1000u / def_.frameRate;
    if (msPerFrame == 0) msPerFrame = 1;

    // First tick after start/resume: advance immediately and anchor time.
    if (lastTickMs_ == 0) {
        lastTickMs_ = nowMs;
        // Don't advance on the very first tick — the first frame is already set.
        return false;
    }

    uint32_t elapsed = nowMs - lastTickMs_;
    if (elapsed < msPerFrame) return false;

    // Advance frame(s) — may skip frames if rendering is very late.
    uint8_t advances = (uint8_t)(elapsed / msPerFrame);
    if (advances > def_.frameCount) advances = def_.frameCount;
    lastTickMs_ = nowMs - (elapsed % msPerFrame);  // keep phase

    for (uint8_t i = 0; i < advances; i++) {
        if (frame_ + 1 >= def_.frameCount) {
            if (def_.loop) {
                frame_ = 0;
            } else {
                frame_ = def_.frameCount - 1;
                playing_ = false;
                finished_ = true;
                break;
            }
        } else {
            frame_++;
        }
    }

    if (frameCb_) frameCb_(frameCbCtx_);
    return true;
}

} // namespace nema::anim
