#pragma once
// Plan 59 — PAPP folder-based app installer (macOS .app style).
//
// A .papp is a DIRECTORY: my-app.papp/manifest.json + app.js + assets/.
// Portable: copy the folder anywhere under /apps/ or /sd/apps/.
// Scanned recursively on boot and every time AppListScreen opens.
//
// Binary PAPP1 format (installPapp) is still supported for PLP wire transfer.
#include <cstdint>
#include <cstddef>

namespace nema {

class Runtime;
struct PappPackage;

// Install from binary PAPP1 data (PLP wire transfer).
bool installPapp(Runtime& rt, const uint8_t* data, size_t len);

// Explode a PAPP1 bundle into a /apps/<id>.papp/ folder.
bool installPappBundle(Runtime& rt, const PappPackage& pkg);

// Scan /apps/ and /sd/apps/ recursively for .papp folders/files.
// Safe to call multiple times (skips already-installed apps).
void loadInstalledPapps(Runtime& rt);

} // namespace nema
