#pragma once
// T2 System Animation Icons — converted from Flipper Zero MainMenu assets.
// These are C-array animations loaded into flash (T2 tier).
// Each animation is a looping sequence of 14×14 px frames (except spinner: 8×8).
// Passive/active = 0 (simple loop, no framesOrder).
#include "nema/ui/animation.h"

namespace nema::assets {

// ── Raw frame bitmaps ────────────────────────────────────────────────────────

#include "anims/anim_icon_settings_raw.h"
#include "anims/anim_icon_badusb_raw.h"
#include "anims/anim_icon_nfc_raw.h"
#include "anims/anim_icon_infrared_raw.h"
#include "anims/anim_icon_apps_raw.h"
#include "anims/anim_spinner_raw.h"

// ── AnimationFrame tables ────────────────────────────────────────────────────

static const nema::anim::AnimationFrame kAnimIconSettingsFrames[] = {
    { kAnimIconSettingsFF0, 14, 14 },
    { kAnimIconSettingsFF1, 14, 14 },
    { kAnimIconSettingsFF2, 14, 14 },
    { kAnimIconSettingsFF3, 14, 14 },
    { kAnimIconSettingsFF4, 14, 14 },
    { kAnimIconSettingsFF5, 14, 14 },
    { kAnimIconSettingsFF6, 14, 14 },
    { kAnimIconSettingsFF7, 14, 14 },
    { kAnimIconSettingsFF8, 14, 14 },
    { kAnimIconSettingsFF9, 14, 14 },
};

static const nema::anim::AnimationFrame kAnimIconBadusbFrames[] = {
    { kAnimIconBadusbFF0,  14, 14 },
    { kAnimIconBadusbFF1,  14, 14 },
    { kAnimIconBadusbFF2,  14, 14 },
    { kAnimIconBadusbFF3,  14, 14 },
    { kAnimIconBadusbFF4,  14, 14 },
    { kAnimIconBadusbFF5,  14, 14 },
    { kAnimIconBadusbFF6,  14, 14 },
    { kAnimIconBadusbFF7,  14, 14 },
    { kAnimIconBadusbFF8,  14, 14 },
    { kAnimIconBadusbFF9,  14, 14 },
    { kAnimIconBadusbFF10, 14, 14 },
};

static const nema::anim::AnimationFrame kAnimIconNfcFrames[] = {
    { kAnimIconNfcFF0, 14, 14 },
    { kAnimIconNfcFF1, 14, 14 },
    { kAnimIconNfcFF2, 14, 14 },
    { kAnimIconNfcFF3, 14, 14 },
};

static const nema::anim::AnimationFrame kAnimIconInfraredFrames[] = {
    { kAnimIconInfraredFF0, 14, 14 },
    { kAnimIconInfraredFF1, 14, 14 },
    { kAnimIconInfraredFF2, 14, 14 },
    { kAnimIconInfraredFF3, 14, 14 },
    { kAnimIconInfraredFF4, 14, 14 },
    { kAnimIconInfraredFF5, 14, 14 },
};

static const nema::anim::AnimationFrame kAnimIconAppsFrames[] = {
    { kAnimIconAppsFF0, 14, 14 },
    { kAnimIconAppsFF1, 14, 14 },
    { kAnimIconAppsFF2, 14, 14 },
    { kAnimIconAppsFF3, 14, 14 },
    { kAnimIconAppsFF4, 14, 14 },
    { kAnimIconAppsFF5, 14, 14 },
    { kAnimIconAppsFF6, 14, 14 },
    { kAnimIconAppsFF7, 14, 14 },
    { kAnimIconAppsFF8, 14, 14 },
};

static const nema::anim::AnimationFrame kAnimSpinnerFrames[] = {
    { kAnimSpinnerFF0, 8, 8 },
    { kAnimSpinnerFF1, 8, 8 },
    { kAnimSpinnerFF2, 8, 8 },
    { kAnimSpinnerFF3, 8, 8 },
    { kAnimSpinnerFF4, 8, 8 },
};

// ── Animation descriptors (T2: C-array, no framesOrder, simple loop) ─────────

inline constexpr nema::anim::Animation animIconSettings = {
    kAnimIconSettingsFrames, 10, 2, true, nullptr, 0, 0, 0
};
inline constexpr nema::anim::Animation animIconBadusb = {
    kAnimIconBadusbFrames, 11, 2, true, nullptr, 0, 0, 0
};
inline constexpr nema::anim::Animation animIconNfc = {
    kAnimIconNfcFrames, 4, 2, true, nullptr, 0, 0, 0
};
inline constexpr nema::anim::Animation animIconInfrared = {
    kAnimIconInfraredFrames, 6, 2, true, nullptr, 0, 0, 0
};
inline constexpr nema::anim::Animation animIconApps = {
    kAnimIconAppsFrames, 9, 2, true, nullptr, 0, 0, 0
};
inline constexpr nema::anim::Animation animSpinner = {
    kAnimSpinnerFrames, 5, 4, true, nullptr, 0, 0, 0
};

} // namespace nema::assets
