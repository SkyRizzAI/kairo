#pragma once
#include "nema/input/input_action.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/ui_sdk.h"

// IDisplayServer (Plan 43) — a swappable rendering backend.
//
// A display server's job is to turn the active view (the ViewDispatcher's screen
// stack) into pixels on a Canvas. Today there is one built-in backend,
// AetherServer (the 1-bit canvas UI). Future backends (fbcon text console,
// LVGL for colour boards) implement the same interface, so the renderer can be
// swapped without touching screens/apps — they only ever produce the view tree.
//
// Plan 50: each server MAY expose a UI SDK (namespace + widget bindings).
// FbconServer returns nullptr (headless-surface); AetherServer returns "aether:ui".
// The loader uses this to wire imports when launching apps.

namespace nema {

class Canvas;
class ViewDispatcher;
struct StatusBarData;
struct UiSdkDescriptor;
struct IUiBindingHost;

struct IDisplayServer {
    virtual ~IDisplayServer() = default;
    virtual const char* name() const = 0;   // "aether" | "fbcon" | "lvgl"

    // Plan 51 — Capability requirements for this server to operate. The launcher
    // checks these before switching; if the board lacks any listed capability the
    // switch is denied and the app gets a user-visible error instead of a black
    // screen. Return nullptr (default) for servers with no requirements (fbcon).
    // Example: AetherServer returns {"display"} (needs a pixel-addressable display).
    virtual const char* const* requiredCaps() const { return nullptr; }

    // Render one frame of the active view to the canvas and flush it. Called by
    // the UI loop only when a redraw is pending and the display is awake.
    virtual void renderFrame(Canvas& c, ViewDispatcher& views, const StatusBarData& status) = 0;

    // Deliver an input action to the server. Returns true if the server consumed
    // the action (GuiService will not forward it to the view dispatcher).
    virtual bool onAction(input::Action) { return false; }

    // ── Presentational state (owned by each server) ───────────────────
    // GuiService installs these into the canvas/theme system before each
    // renderFrame() call. Changing settings in one server never bleeds
    // into another — every server owns its own visual identity.

    // Theme (Plan 53): default → nema::defaultTheme() at render time.
    const StyleTokens* serverTheme() const { return serverTheme_; }
    void setServerTheme(const StyleTokens& t) { serverTheme_ = &t; }

    // Canvas logical scale: how many physical pixels per logical pixel.
    // 0.0 = not set by this server; GuiService keeps the canvas as-is.
    float serverScale() const { return serverScale_; }
    void  setServerScale(float s) { serverScale_ = (s >= 1.0f) ? s : 1.0f; }

    // ── Plan 50: UI SDK ───────────────────────────────────────────────

    // UI SDK this server exposes. nullptr = headless-surface (fbcon):
    // no widget namespace to import.
    virtual const UiSdkDescriptor* uiSdk() const { return nullptr; }

    // Wire this server's UI SDK functions into a runtime import table.
    // Called by the loader when launching an app targeting this server.
    // Default: no-op (server has no SDK or is in tree-only mode).
    virtual void registerBindings(IUiBindingHost&) {}

private:
    const StyleTokens* serverTheme_ = nullptr;
    float              serverScale_  = 1.0f;   // default: no scaling
};

} // namespace nema
