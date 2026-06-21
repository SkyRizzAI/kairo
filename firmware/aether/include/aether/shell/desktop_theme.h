#pragma once
// Plan 81 — Desktop skin interface (the idle wallpaper screen).
//
// A desktop theme paints a full-screen (or content-area) wallpaper, optionally
// animated. The wallpaper bitmap rarely matches the panel, so placement is
// configurable via a fit mode + a 9-grid anchor (persisted under the "desktop"
// config namespace and edited in DesktopSettingScreen).
#include <cstdint>

namespace nema { class Canvas; }
namespace nema::anim { class AnimationPlayer; }

namespace nema::shell {

// How the wallpaper frame fills the target rect.
enum class FitMode : uint8_t {
    Center,   // no scaling — native size, placed by anchor
    Stretch,  // scale X/Y independently to fill (aspect ignored)
    Crop,     // uniform scale to COVER the rect; overflow clipped (anchor biases)
    Fit,      // uniform scale to CONTAIN inside the rect; letterboxed (anchor biases)
};

// 9-grid placement. Used by Center (position) and by Crop/Fit (which overflow /
// letterbox edge to bias toward).
enum class Anchor : uint8_t {
    TopLeft, Top, TopRight,
    Left,    Center, Right,
    BottomLeft, Bottom, BottomRight,
};

constexpr int kFitCount    = 4;
constexpr int kAnchorCount = 9;

// Enum ↔ config-string mapping (shared by the skin and DesktopSettingScreen so
// the persisted value and the cycle list never drift).
const char* fitName(FitMode m);          // "center" | "stretch" | "crop" | "fit"
FitMode     fitFromName(const char* s, FitMode fallback = FitMode::Center);
const char* anchorName(Anchor a);        // "center" | "top-left" | ...
Anchor      anchorFromName(const char* s, Anchor fallback = Anchor::Center);
const char* const* fitNames();           // kFitCount entries (cycle order)
const char* const* anchorNames();        // kAnchorCount entries (cycle order)

// A desktop skin. The screen ticks it (for animation) and asks it to draw into a
// target rect (full canvas when status bar is off, content area when it's on).
struct IDesktopTheme {
    virtual ~IDesktopTheme() = default;
    virtual const char* name() const = 0;             // "livewal"
    virtual void onResume() {}                          // re-read config
    virtual void tick(uint32_t /*nowMs*/) {}            // advance animation
    virtual void draw(nema::Canvas& c,
                      uint16_t x, uint16_t y, uint16_t w, uint16_t h) = 0;
    // Animation player to register with AnimationManager (null = static skin).
    virtual nema::anim::AnimationPlayer* player() { return nullptr; }
};

} // namespace nema::shell
