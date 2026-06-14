#pragma once

namespace nema {

// How an app got onto the device. BuiltIn apps are compiled into the firmware
// and installed at boot (the ".fam" equivalent — see AppRegistry). Custom apps
// are installed at runtime — a .kapp package pushed over the wire (OTA via PLP)
// or loaded from storage (flash/SD, Plan 38). Same launcher, same registry;
// only the origin differs.
enum class AppKind { BuiltIn, Custom };

// What the entry IS. An App is launchable: shown in the launcher, spawned on its
// own Nema thread (or pushed as a screen) when selected. A Service is a
// background daemon: hidden from the launcher, started at boot (or immediately
// when installed while running) and ticked by the ServiceManager. Mirrors
// Flipper's apptype=APP/SERVICE — both live in the same manifest table.
enum class AppType { App, Service };

// AppManifest — an installed entry's header: the metadata the launcher and
// system need without touching the entry's code. For built-ins it's derived
// from the app (id/name); for custom apps it's parsed from the package's
// kapp.json. This is the on-device twin of packages/nema-app-sdk kapp.json.
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
};

} // namespace nema
