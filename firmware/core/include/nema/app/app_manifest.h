#pragma once
#include "nema/app/runtime_tier.h"
#include <cstdint>

namespace nema {

// How an app got onto the device. BuiltIn apps are compiled into the firmware
// and installed at boot (the ".fam" equivalent — see AppRegistry). Custom apps
// are installed at runtime — a .papp package pushed over the wire (OTA via PLP)
// or loaded from storage (flash/SD, Plan 38). Same launcher, same registry;
// only the origin differs.
enum class AppKind { BuiltIn, Custom };

// What the entry IS. An App is launchable: shown in the launcher, spawned on its
// own Nema thread (or pushed as a screen) when selected. A Service is a
// background daemon: hidden from the launcher, started at boot (or immediately
// when installed while running) and ticked by the ServiceManager. Mirrors
// Flipper's apptype=APP/SERVICE — both live in the same manifest table.
enum class AppType { App, Service };

// Plan 59 — app execution mode (orthogonal to runtime tier and display server).
//   Cli    — stdio only; no surface needed; pipe-able.
//   Ui     — requires a surface (display server must be available).
//   Hybrid — can run headless or lift a UI window, decided at runtime.
enum class AppMode { Cli, Ui, Hybrid };

// AppManifest — an installed entry's header: the metadata the launcher and
// system need without touching the entry's code. For built-ins it's derived
// from the app (id/name); for custom apps it's parsed from the package's
// papp.json (Plan 59 §1). This is the on-device twin of manifest.json.
//
// Strings are non-owning (the same lifetime contract as IApp::id()/name()): a
// built-in points at string literals, a custom app at the JsApp's std::strings,
// which outlive the registry entry.
struct AppManifest {
    const char* id;        // "com.palanu.clock"
    const char* name;      // "Clock"
    const char* version;   // "1.0.0"
    AppKind     kind;      // BuiltIn (compiled-in) or Custom (installed at runtime)
    AppType     type;      // App (launchable) or Service (background daemon)

    // Plan 56 — which execution sandbox this app runs in.
    RuntimeTier runtimeTier = RuntimeTier::CBuiltin;

    // Plan 51 — preferred display server ("aether", "fbcon", or nullptr = any).
    // GuiService switches to this server before the app thread starts.
    const char* displayServer = nullptr;

    // Plan 59 — extended manifest fields (parsed from papp.json for custom apps;
    // defaults for built-ins).

    // cli | ui | hybrid (default Ui for display-server apps, Cli for headless).
    AppMode mode = AppMode::Ui;

    // Launcher group label (default "Apps").
    const char* category = "Apps";

    // Icon handle (Plan 53 icon system — e.g. "feature.apps") or bundle-relative
    // path ("icons/app.xbm"). nullptr = generic icon per runtimeTier.
    const char* iconPath = nullptr;

    // Plan 84 — custom bitmap icon loaded from .papp bundle (icon.raw).
    // Format: 4-byte header (width u16le, height u16le) + 1-bit packed pixels
    // (MSB first, row-major, stride = ceil(w/8)). Pointer is non-owning —
    // the JsApp that installed this app owns the buffer and outlives the manifest.
    // nullptr = no custom icon; fall back to iconPath / icon_pack.
    const uint8_t* iconBitmap = nullptr;
    uint8_t        iconW      = 0;
    uint8_t        iconH      = 0;

    // Null-terminated array of required capability strings ("net.http", "storage",
    // …). nullptr or empty = no requirements. Checked against device capabilities
    // at launch; missing capability → launch rejected with a user-visible error.
    const char* const* needs = nullptr;

    // Target System API version ("major.minor", Plan 48). Major mismatch vs host
    // api_version → install rejected. nullptr = "1.0".
    const char* apiVersion = "1.0";

    // Plan 83 — storage management hints.
    // storageMovable: user can move this app's data to SD card via Storage settings.
    // hasCriticalData: app stores credentials/keys that must always stay on internal
    //   flash (e.g. security apps). Setting this to true adds a warning in the UI
    //   that some data cannot be moved, even if storageMovable is true.
    bool storageMovable   = true;
    bool hasCriticalData  = false;
};

} // namespace nema
