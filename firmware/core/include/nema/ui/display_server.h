#pragma once

// IDisplayServer (Plan 43) — a swappable rendering backend.
//
// A display server's job is to turn the active view (the ViewDispatcher's screen
// stack) into pixels on a Canvas. Today there is one built-in backend,
// AetherServer (the 1-bit canvas UI). Future backends (fbcon text console,
// LVGL for colour boards) implement the same interface, so the renderer can be
// swapped without touching screens/apps — they only ever produce the view tree.
//
// Fase 1 keeps GuiService as the owner of the UI loop + input + DPM; the server
// is just the render step. Later phases (DisplayManager) move loop ownership and
// add mount/unmount + input delivery so servers can be started/stopped from the
// CLI at runtime.

namespace nema {

class Canvas;
class ViewDispatcher;
struct StatusBarData;

struct IDisplayServer {
    virtual ~IDisplayServer() = default;
    virtual const char* name() const = 0;   // "aether" | "fbcon" | "lvgl"

    // Render one frame of the active view to the canvas and flush it. Called by
    // the UI loop only when a redraw is pending and the display is awake.
    virtual void renderFrame(Canvas& c, ViewDispatcher& views, const StatusBarData& status) = 0;
};

} // namespace nema
