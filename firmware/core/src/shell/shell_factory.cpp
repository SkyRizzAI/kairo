// Plan 81 — Shell factory: config name → concrete skin. The ONLY place that
// knows the concrete classes; adding a skin is one new include + one line here.
#include "nema/shell/shell_factory.h"
#include "nema/shell/desktop_livewall.h"
#include "nema/shell/launcher_playstation.h"
#include "nema/shell/launcher_wii.h"
#include <cstring>

namespace nema::shell {

std::unique_ptr<IDesktopTheme> makeDesktop(const char* name, nema::Runtime& rt) {
    // Only one desktop skin for now; everything falls back to it.
    (void)name;
    return std::make_unique<LiveWallpaperDesktop>(rt);
}

std::unique_ptr<ILauncherTheme> makeLauncher(const char* name) {
    if (name && std::strcmp(name, "wii") == 0)
        return std::make_unique<WiiLauncher>();
    return std::make_unique<PlayStationLauncher>();   // default
}

} // namespace nema::shell
