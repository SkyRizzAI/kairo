// Plan 81 — FitMode/Anchor ↔ config-string tables, shared by the wallpaper skin
// and DesktopSettingScreen so the persisted value and the cycle list never drift.
#include "aether/shell/desktop_theme.h"
#include <cstring>

namespace nema::shell {

static const char* kFitNames[kFitCount] = { "center", "stretch", "crop", "fit" };
static const char* kAnchorNames[kAnchorCount] = {
    "top-left", "top", "top-right",
    "left",     "center", "right",
    "bot-left", "bottom", "bot-right",
};

const char* const* fitNames()    { return kFitNames; }
const char* const* anchorNames() { return kAnchorNames; }

const char* fitName(FitMode m) {
    int i = (int)m;
    return (i >= 0 && i < kFitCount) ? kFitNames[i] : kFitNames[0];
}
FitMode fitFromName(const char* s, FitMode fallback) {
    if (s) for (int i = 0; i < kFitCount; i++)
        if (std::strcmp(s, kFitNames[i]) == 0) return (FitMode)i;
    return fallback;
}

const char* anchorName(Anchor a) {
    int i = (int)a;
    return (i >= 0 && i < kAnchorCount) ? kAnchorNames[i] : kAnchorNames[4];
}
Anchor anchorFromName(const char* s, Anchor fallback) {
    if (s) for (int i = 0; i < kAnchorCount; i++)
        if (std::strcmp(s, kAnchorNames[i]) == 0) return (Anchor)i;
    return fallback;
}

} // namespace nema::shell
