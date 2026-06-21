#include "nema/ui/animation_player.h"

namespace nema::anim {

AnimationPlayer::AnimationPlayer(const Animation& def)
    : def_(def) {}

// ── helpers ───────────────────────────────────────────────────────────────────

uint8_t AnimationPlayer::playheadLen() const {
    if (def_.framesOrder && def_.framesOrderLen > 0)
        return def_.framesOrderLen;
    return def_.frameCount;
}

uint8_t AnimationPlayer::resolveFrame() const {
    if (def_.frameCount == 0) return 0;
    uint8_t len = playheadLen();
    uint8_t ph  = (len > 0) ? playhead_ % len : 0;
    if (def_.framesOrder && def_.framesOrderLen > 0)
        return def_.framesOrder[ph] % def_.frameCount;
    return ph % def_.frameCount;
}

void AnimationPlayer::advancePlayhead() {
    uint8_t len = playheadLen();
    if (len == 0) return;

    if (inActive_) {
        // Active segment: passiveCount … passiveCount+activeCount-1
        uint8_t activeEnd = (uint8_t)(def_.passiveCount + def_.activeCount);
        if (activeEnd > len) activeEnd = len;

        if (playhead_ + 1 < activeEnd) {
            playhead_++;
        } else {
            // Active done — return to start of passive loop
            inActive_  = false;
            playhead_  = 0;
        }
        return;
    }

    // Passive (or plain sequential) loop
    uint8_t passiveEnd = (def_.passiveCount > 0) ? def_.passiveCount : len;
    if (passiveEnd > len) passiveEnd = len;

    if (playhead_ + 1 < passiveEnd) {
        playhead_++;
    } else if (def_.loop) {
        playhead_ = 0;
    } else {
        playhead_ = (uint8_t)(passiveEnd - 1);
        playing_  = false;
        finished_ = true;
    }
}

// ── public API ────────────────────────────────────────────────────────────────

const uint8_t* AnimationPlayer::currentFrameData() const {
    if (def_.frameCount == 0) return nullptr;
    return currentDef().bitmap;
}

void AnimationPlayer::start() {
    if (def_.frameCount == 0) return;
    playing_    = true;
    finished_   = false;
    inActive_   = false;
    playhead_   = 0;
    lastTickMs_ = 0;
}

void AnimationPlayer::stop() {
    playing_    = false;
    finished_   = false;
    inActive_   = false;
    playhead_   = 0;
}

void AnimationPlayer::pause() {
    playing_ = false;
}

void AnimationPlayer::triggerActive() {
    if (!playing_ || def_.activeCount == 0) return;
    uint8_t len = playheadLen();
    uint8_t activeStart = def_.passiveCount;
    if (activeStart >= len) return;
    inActive_ = true;
    playhead_ = activeStart;
}

bool AnimationPlayer::tick(uint32_t nowMs) {
    if (!playing_ || def_.frameCount == 0 || def_.frameRate == 0) return false;

    uint32_t msPerFrame = 1000u / def_.frameRate;
    if (msPerFrame == 0) msPerFrame = 1;

    if (lastTickMs_ == 0) {
        lastTickMs_ = nowMs;
        return false;
    }

    uint32_t elapsed = nowMs - lastTickMs_;
    if (elapsed < msPerFrame) return false;

    uint8_t advances = (uint8_t)(elapsed / msPerFrame);
    uint8_t len      = playheadLen();
    if (advances > len) advances = len;
    lastTickMs_ = nowMs - (elapsed % msPerFrame);

    for (uint8_t i = 0; i < advances; i++)
        advancePlayhead();

    if (frameCb_) frameCb_(frameCbCtx_);
    return true;
}

} // namespace nema::anim
