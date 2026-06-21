#pragma once
// Plan 81 — Shell factory: maps a config name → a concrete desktop/launcher skin.
// Adding a skin = one new class + one line here.
#include "aether/shell/desktop_theme.h"
#include "aether/shell/launcher_theme.h"
#include <memory>

namespace nema { class Runtime; }

namespace nema::shell {

// Defaults used when config is empty or the name is unknown.
constexpr const char* kDefaultDesktop  = "livewal";
constexpr const char* kDefaultLauncher = "playsta";

// name fallbacks to the default skin. Desktop needs Runtime (config: fit/anchor).
std::unique_ptr<IDesktopTheme>  makeDesktop(const char* name, nema::Runtime& rt);
std::unique_ptr<ILauncherTheme> makeLauncher(const char* name);

} // namespace nema::shell
