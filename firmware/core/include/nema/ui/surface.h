#pragma once
#include <cstdint>

// Plan 55 — ISurface: the drawing surface a foreground app gets from the display
// server. Decouples canvas/present/input from AppContext so future backends
// (WASM, lvgl) can implement a surface without inheriting AppHost.
//
// Lifecycle:
//   The surface is provided to the app before run() is called.
//   canvas()    — draw into this (app thread only).
//   present()   — publish the frame; GUI thread blits it next cycle.
//   nextInput() — non-blocking pop from the app's input mailbox.
//   waitInput() — blocking pop up to timeoutMs.
//
// AppHost currently implements AppContext (ISurface + ProcessContext) and
// IScreen in one class. Plan 55 full migration: AppHost wraps an ISurface
// implementation rather than being one. The interface is defined here so
// code can forward-declare and depend on it independently.

namespace nema {

class Canvas;
struct InputEvent;

class ISurface {
public:
    virtual ~ISurface() = default;

    // Draw target. Valid on the app thread between present() calls.
    virtual Canvas& canvas() = 0;

    // Publish the current frame. The GUI thread blits the buffer next render.
    virtual void present() = 0;

    // Pop one input event (non-blocking). Returns false if mailbox is empty.
    virtual bool nextInput(InputEvent& out) = 0;

    // Block up to timeoutMs for an input event. Returns false on timeout.
    virtual bool waitInput(InputEvent& out, uint32_t timeoutMs) = 0;

    // Plan 86 Fase 2 — flip from Terminal to Gui mode. Called by the first
    // canvas_* or ui_* WASM import. No-op by default (ProcessHost has no mode).
    virtual void enterGuiMode() {}

    // True once the app has flipped to Gui mode (drew to the canvas) rather than
    // printing terminal output. A GUI app that returns from main() should exit
    // straight back to the launcher, not park on the "Press any key to exit"
    // terminal screen (which is meant for CLI apps whose output you read).
    virtual bool isGuiMode() const { return false; }
};

} // namespace nema
