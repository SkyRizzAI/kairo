# Palanu — Project Conventions

Palanu is a hardware-agnostic embedded firmware runtime (C++17). One core, many
boards. Code is layered: **core** (`firmware/core/`, no hardware deps) →
**platforms** (`esp32`, `simulator`) → **boards** (`dev-board`, `skyrizz-e32`) →
**targets** (`firmware/targets/*` — the buildable apps).

These are project-wide rules. They apply to **every board and every layer** — not
per-board taste. When adding code, follow them; when you see a violation, fix it.

---

## Logging

**All system/application logging MUST go through the Palanu Logger (`rt.log()`).**
Never use raw `Serial`, `printf`/`fprintf`/`std::cout`, or `ESP_LOGx` for logging.

```cpp
rt.log().info ("Component", "human message");
rt.log().info ("Xl9535",    "started", {{"addr", "0x20"}, {"int", "GPIO43"}});
rt.log().error("LcdDriver", "framebuf alloc failed");
// Levels: trace / debug / info / warn / error / fatal
```

- **Component tag** = the subsystem/class name (`"LcdDriver"`, `"Xl9535"`,
  `"SkyRizzE32"`, `"GuiService"`). Keep it stable so logs are filterable.
- Structured fields are `{{"key", "value"}, ...}` — prefer them over string
  concatenation for machine-readable data.

### Why this is mandatory

The Logger is not just a `printf` wrapper. A single `rt.log()` call fans out to
**multiple sinks**:
- **ConsoleSink** → `stdout` (human format `[HH:MM:SS] [LEVEL] [Component] msg`).
  On hardware this reaches the USB serial; on the simulator, the host console.
- **MemorySink** → in-memory ring buffer that the on-device **Logs screen** and
  simulator telemetry read.

Raw `Serial`/`printf` bypass the sinks: no level, no tag, no Logs-screen capture,
no consistent format. That fragments the system's observability.

> Note on transport: `Serial`, the IDF console (`I (xxx) ...`), and `rt.log()`
> all end up on the **same physical USB serial wire** — they are different
> *software layers*, not different pipes. Using `Serial` directly doesn't reach a
> "different place"; it just skips the Logger's structure and sinks.

### Sanctioned exceptions (the ONLY places raw stdio is allowed)

1. **Pre-runtime boot banner** in `targets/*/main.cpp`: before `rt.initCore()`
   the Logger does not exist yet, so a single raw `Serial.println("booting...")`
   is allowed. **After `initCore()`, switch to `rt.log()`.**
2. **`firmware/tests/`**: test harnesses print pass/fail to stdout directly.

(The old native simulator's stdio telemetry bridge was removed — the simulator
is now WASM, which speaks KLP over a virtual cable, not raw stdout.)

Anywhere else, raw stdio for logging is a bug.

---

## Hardware abstraction

- **Check capabilities, never board type.** Use
  `rt.capabilities().has("wifi")`, `has("input.2d")`, etc. Never branch on board
  name or `#ifdef ESP32` in core/app code. Boards declare capabilities in
  `describeHardware()`.
- A new board = `firmware/boards/<name>/` (drivers + `IBoard`) plus a
  `firmware/targets/<name>/` (the buildable project). The **pin map is the single
  source of truth** in the board's `board_config.h` — mirror the hardware doc, no
  magic numbers scattered in drivers.

## Input

- Screens/apps program against **`input::Action`** (`onAction()`) — the
  hardware-agnostic intent layer (Prev / Next / Activate / Back / …). Do not
  consume raw physical `Key`/`Code` unless the physical identity genuinely
  matters.
- Each board provides exactly one **`IKeyMap`** that translates physical buttons
  (with gestures: short/long/double/chord) into Code + Action, and must pass
  `validateFloor()` (Prev, Next, Activate, Back all reachable).
- Footer/hint labels come from `rt.input().hintFor(Action)` — never hardcode
  button names like `"Cancel"` in screen code (they'd be wrong on other boards).

## UI / layout

- Resolution-independent: draw from `canvas.width()`/`canvas.height()`, never
  hardcode screen dimensions. (Plan 25 — Adaptive UI.)
