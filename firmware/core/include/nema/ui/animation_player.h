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

    bool isPlaying()   const { return playing_; }
    bool isLastFrame() const { return playhead_ + 1 >= playheadLen(); }

    uint8_t        currentFrameIndex() const { return resolveFrame(); }
    const uint8_t* currentFrameData()  const;
    uint16_t       width()  const { return currentDef().width; }
    uint16_t       height() const { return currentDef().height; }

    // Trigger the active segment (if activeCount > 0). No-op if already active
    // or if the animation has no active frames.
    void triggerActive();

    // Callback fired on every frame advance.
    void setFrameCallback(AnimationFrameCallback cb, void* ctx) {
        frameCb_ = cb; frameCbCtx_ = ctx;
    }

    // Tick — called by AnimationManager. Returns true if frame changed.
    bool tick(uint32_t nowMs);

private:
    uint8_t resolveFrame()  const;  // playhead → bitmap index
    uint8_t playheadLen()   const;  // total playback positions (order or frameCount)
    void    advancePlayhead();

    const AnimationFrame& currentDef() const { return def_.frames[resolveFrame()]; }

    const Animation& def_;
    uint8_t          playhead_   = 0;
    bool             playing_    = false;
    bool             finished_   = false;
    bool             inActive_   = false;
    uint32_t         lastTickMs_ = 0;

    AnimationFrameCallback frameCb_    = nullptr;
    void*                  frameCbCtx_ = nullptr;
};

} // namespace nema::anim
