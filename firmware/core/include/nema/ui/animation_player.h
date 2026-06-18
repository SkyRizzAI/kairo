#pragma once
#include "nema/ui/animation.h"

namespace nema::anim {

// Callback type for per-frame notifications (used to request redraw).
using AnimationFrameCallback = void (*)(void* context);

// Plan 70: Runtime animation player — owns a single Animation instance and
// tracks its playback state (current frame, playing/paused/stopped, last tick).
//
// The AnimationManager calls tick() each frame; when the frame advances, the
// callback fires so the owning View can mark itself dirty.
//
// Thread safety: AnimationPlayer is designed to be touched only from the GUI
// thread (the same thread that runs GuiService::loop and the AnimationManager).
class AnimationPlayer {
public:
    explicit AnimationPlayer(const Animation& def);

    // Control
    void start();                                  // start/resume playback
    void stop();                                   // stop and reset to frame 0
    void pause();                                  // stop but keep current frame

    bool isPlaying() const { return playing_; }
    bool isLastFrame() const { return frame_ >= def_.frameCount - 1; }

    uint8_t      currentFrameIndex() const { return frame_; }
    const uint8_t* currentFrameData() const;       // XBM data for current frame
    uint16_t     width()  const { return currentDef().width; }
    uint16_t     height() const { return currentDef().height; }

    // Callback — called whenever the frame advances (set from GUI thread).
    void setFrameCallback(AnimationFrameCallback cb, void* ctx) {
        frameCb_ = cb; frameCbCtx_ = ctx;
    }

    // Tick — called by AnimationManager each frame with the current timestamp (ms).
    // Advances the frame if enough time has passed since the last advance.
    // Returns true if the frame changed (caller should redraw).
    bool tick(uint32_t nowMs);

private:
    const AnimationFrame& currentDef() const { return def_.frames[frame_]; }

    const Animation& def_;
    uint8_t          frame_     = 0;
    bool             playing_   = false;
    bool             finished_  = false;   // one-shot: stopped after last frame
    uint32_t         lastTickMs_ = 0;

    AnimationFrameCallback frameCb_    = nullptr;
    void*                  frameCbCtx_ = nullptr;
};

} // namespace nema::anim
