# 18 — 1:1 runtime parity for UI components (core C++ / WASM / JS apps)

- **Status:** Accepted
- **Date:** 2026-06-27
- **Area:** ui/aether, core/js (reconciler), app-sdk
- Relates to: [ADR 0006](0006-wasm-app-dispatch-via-iapp-not-iappruntime.md) (WASM app dispatch)

## Context

The Aether UI is not only the firmware's internal toolkit — it is a **product feature
for third-party app developers**. Apps ship as JavaScript (`.papp`) bundles rendered by
the on-device JS engine, and developers build/preview them against the WASM simulator
(the same core compiled to WebAssembly). That makes UI components a public API surface,
not an implementation detail.

The consequence: any component the built-in firmware screens use must be **equally
available to JS apps and to the simulator**. If the settings screen gets a graphical
on/off switch but a `.papp` can only draw a text `[ON]/[OFF]` toggle, the two diverge and
the simulator stops being a faithful preview. There are three runtimes that must agree:

- **Core C++** screens/apps (native, on hardware).
- **WASM simulator** — the same core compiled to WASM.
- **JS `.papp` apps** — driven by the on-device JS engine's reconciler.

## Decision

Components live **once, in core C++**, and reach all three runtimes through one path each:

1. **Core C++** — a component is either a native `NodeType` (drawn by the renderer) or a
   builder in `widgets.h`/`widgets.cpp` that composes primitives. This is the single
   implementation.

2. **WASM simulator** — gets every component **for free**: it is the same core compiled
   to WebAssembly, so a native `NodeType` or builder is already present. No simulator-side
   work.

3. **JS apps** — two pieces, both required:
   - **`js_engine.cpp` reconciler** (`JsEngine::reify`): maps the element-type *string*
     from the JS tree to a `NodeType` and copies props onto the `UiNode` (e.g.
     `"Switch"` → `NodeType::Switch` + `switchOn`, `"ProgressBar"` → `NodeType::Progress`
     + `progressPct`, clamped 0..100, with sensible default `style.width/height`).
   - **`app-sdk` `components.ts`**: exposes a typed intrinsic marker (`intrinsic("Switch")`
     tagged via `fn.__tag`) and a props interface, so app authors get a real component
     with editor types.

**`Switch` and `ProgressBar` were promoted to native `NodeTypes`** (rather than left as
primitive compositions) **specifically** so that one renderer implementation backs all
three runtimes — the firmware switch, the simulator switch, and the `.papp` switch are
pixel-identical because they are literally the same draw code.

## Consequences

- **Adding a shared component is a three-place checklist.** To keep parity, a new
  component that JS apps should use must be added in all of:
  1. **Core** — the `NodeType` + renderer draw code (`node.h`, `renderer.cpp`), or a
     builder in `widgets.h`/`widgets.cpp`.
  2. **`js_engine.cpp` reconciler** — a `type == "Name"` branch in `JsEngine::reify` that
     sets the node type and maps props (with clamping + default sizing).
  3. **`app-sdk/src/components.ts`** — an `intrinsic("Name")` export plus a typed
     `Props` interface.
  Skipping (2) or (3) means the component works on hardware/simulator but is invisible or
  untyped to app developers — silent divergence.
- **WASM needs nothing extra** — it tracks core automatically. The parity burden is
  entirely on the JS-app path (reconciler + SDK).
- **Native node vs. builder is a deliberate choice.** Promote to a native `NodeType` when
  cross-runtime pixel parity or a custom draw (e.g. the switch's knob-hole) matters;
  leave as a builder when it is pure primitive composition. Native nodes cost a renderer
  branch; builders cost arena nodes.
- **Ruled out:** re-implementing components separately per runtime (guarantees drift),
  and exposing core builders to JS without a reconciler mapping (the JS tree only carries
  type strings + props, so an unmapped type falls through to `NodeType::View`).
