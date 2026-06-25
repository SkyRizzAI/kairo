# Palanu — Project Conventions

Palanu is a hardware-agnostic embedded firmware runtime (C++17). One core, many
boards. Code is layered: **core** (`firmware/core/`, no hardware deps) →
**platforms** (`esp32`, `simulator`) → **boards** (`dev-board`, `skyrizz-e32`) →
**targets** (`firmware/targets/*` — the buildable apps).

These are project-wide rules. They apply to **every board and every layer** — not
per-board taste. When adding code, follow them; when you see a violation, fix it.

---

## Documentation — automated upkeep

Docs are written and maintained **by the AI as part of doing the work**, not as a
separate chore. There is no command to run — these rules govern it. **Write all docs
in English.**

### Doc map (one source of truth per fact — never duplicate facts across files)

| File / folder | Purpose | Mutability |
|---|---|---|
| [`docs/STATE.md`](docs/STATE.md) | Living snapshot of project status. A **rollup**, not detail. Read it first. | mutable |
| [`docs/overview.md`](docs/overview.md) | Single-file narrative snapshot of the whole project. | mutable |
| [`docs/architecture/`](docs/architecture/README.md) | Detailed per-subsystem reference (the "how it works"). Update when a subsystem's design changes. | mutable |
| [`docs/plans/NN-*.md`](docs/plans/00-overview.md) | Planning per stage. **Task checklists live here** (`- [ ]`), not in a separate file. | mutable until shipped |
| [`docs/decisions/`](docs/decisions/) | ADRs — one decision per file, numbered, **append-only & immutable**. | immutable |
| [`docs/feats/`](docs/feats/) | Feature reference — how a feature works *now* (not its history). | mutable |
| [`CHANGELOG.md`](CHANGELOG.md) | **Auto-generated from conventional commits. NEVER hand-edit.** | generated |

### Two roles

- **Planner** (cofounder): writes/revises `docs/plans/`.
- **Builder**: executes a plan, then updates docs per the Definition of Done below.

### Definition of Done (builder — before declaring a task finished)

1. **STATE.md** — if an area's status changed (e.g. `build` → `HW-verified`), update
   its row and the `Last updated` date.
2. **docs/feats/** — if a feature is new or its behavior changed, create/update its file.
3. **docs/decisions/** — if an architectural/non-obvious decision was made, write a new
   ADR (criteria below).
4. **Plan checklist** — tick the `- [x]` tasks completed.
5. **Commit** — use a conventional commit (`feat:`/`fix:`/`docs:`…) so the changelog
   regenerates automatically. Do not edit `CHANGELOG.md` by hand.

### When to write an ADR

Only for decisions a future maintainer would ask "why?" about: an architectural choice,
picking one option among several with a trade-off, or capturing the reasoning behind a
subtle bug fix. **Not** every change. Format: `Context` → `Decision` → `Consequences`.
Number sequentially (`NNNN-kebab-title.md`); never rewrite a shipped ADR — supersede it
with a new one that references the old. See [`docs/decisions/0000-template.md`](docs/decisions/0000-template.md).

> Future tooling (not yet built): `/palanu-plan <topic>` to scaffold a plan, and
> `/palanu-docs` to diff against the last commit and report which docs need updating.

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

## App UX (MANDATORY — applies to every app)

- **An app ALWAYS opens to its home/menu screen — NEVER jump straight into a
  sub-flow or action.** Set the landing state to the menu in `onStart()`. If the
  app has data, the home lists it ("My Wallet", "My Notes", …) with actions
  (Create / Settings / About); if empty, the home offers the entry actions. The
  first thing the user sees is a *choice*, not a screen mid-task.
- **Swallow the launcher's "Activate" on the first frame.** The same `Activate`
  that launched the app from the launcher bleeds into the app's first input and
  fires the first-focused control — so the app appears to "jump" straight into
  whatever button was focused (e.g. a wallet opening directly on the backup-phrase
  screen). Guard it: ignore the first activation after `onStart()`. See
  `WalletsApp::swallowFirst()` and `BadUsbApp::suppressNext_` for the pattern.
- Footer/back behaviour: `Back` from a sub-screen returns to the home menu; `Back`
  from the home exits the app. Don't trap the user.
- **Sub-screen titles: use a `ListSection` subheader, NOT a big `TitleBar` banner.**
  The full-width filled `TitleBar` eats vertical space on the small display; match the
  Settings screens — each screen is a `ListContainer` whose first `ListSection` names it
  (and groups already labelled by their own `ListSection` don't need a separate title).

---

## SkyRizz E32 — USB mode toggle (HID/CDC ↔ JTAG/Serial)

The E32 has two mutually-exclusive USB modes. You switch between them by
commenting/uncommenting **exactly 2 lines** in
`firmware/targets/skyrizz-e32/CMakeLists.txt` (lines 36-37). **Both lines must
match** — never enable one without the other.

| | USB HID/CDC mode | JTAG/Serial mode |
|---|---|---|
| CMake flags | both **uncommented** | both **commented** |
| `ARDUINO_USB_CDC_ON_BOOT` | `1` | `0` (see note ‡) |
| Arduino `Serial` | native USB CDC (TinyUSB) | UART0 (GPIO43/44 — **not wired to host on this board**) |
| BadUSB / HID keyboard | ✅ works (Plan 66) | ❌ TinyUSB not initialized |
| Forge remote desktop | ✅ over USB CDC | ✅ over USB Serial/JTAG (HWCDC) |
| Flashing | needs **manual download mode** † | direct, no button dance (fast dev) |
| Host port | `/dev/cu.usbmodem*` (TinyUSB CDC) | `/dev/cu.usbmodem*` (built-in USB Serial/JTAG) |

† Download mode = power off, hold BOOT, power on, release BOOT, then flash.
‡ See the preprocessor note below — this is why the remote broke once.

### To ENABLE USB HID/CDC mode (BadUSB) — uncomment lines 36-37:
```cmake
idf_build_set_property(COMPILE_OPTIONS "-DARDUINO_USB_CDC_ON_BOOT=1" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DARDUINO_USB_ON_BOOT=1"     APPEND)
```

### To DISABLE it / back to JTAG-Serial (fast flashing) — comment lines 36-37:
```cmake
# idf_build_set_property(COMPILE_OPTIONS "-DARDUINO_USB_CDC_ON_BOOT=1" APPEND)
# idf_build_set_property(COMPILE_OPTIONS "-DARDUINO_USB_ON_BOOT=1"     APPEND)
```

**Current state: JTAG/Serial mode (both lines commented).**

### After EITHER toggle — a clean build is mandatory:
```bash
rm -rf firmware/targets/skyrizz-e32/build && bun run build:skyrizz-e32
```

### Flashing
```bash
# JTAG/Serial mode — direct, no button dance:
idf.py -p /dev/cu.usbmodem* -B firmware/targets/skyrizz-e32/build flash monitor

# USB HID/CDC mode — enter download mode first (power off, hold BOOT, power on,
# release BOOT), then the same flash command.
```

### ‡ Preprocessor gotcha (root cause of the JTAG-mode remote regression)

`Serial` is **not** the right transport for the Forge remote in JTAG mode. The
correct port the host sees is the **built-in USB Serial/JTAG (HWCDC)**, not UART0.
So `firmware/platforms/esp32/src/esp32_usb_cdc.cpp` drives **`HWCDC` directly** when
in JTAG mode and `Serial` (USB CDC) when in HID/CDC mode.

That file MUST select the branch with `#if`, **never `#ifdef`/`#ifndef`**:

```cpp
#include <Arduino.h>          // HardwareSerial.h does: #ifndef … #define ARDUINO_USB_CDC_ON_BOOT 0
#if ARDUINO_USB_CDC_ON_BOOT   // == 1 → USB CDC mode  → use Serial
#else                         // == 0 → JTAG mode     → use HWCDC
#endif
```

arduino-esp32 **always defines** `ARDUINO_USB_CDC_ON_BOOT` (to `0` if no `-D` flag),
so `#ifdef` is always true and `#ifndef` always false — which silently routes the
remote to UART0 (dead on this board) → Forge stuck on "Waiting for device…".
Test the macro's **value** with `#if`, not its existence.

> Note: the secondary console (`CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y` in
> `sdkconfig`) must stay enabled so `rt.log()` output is visible in `idf.py monitor`
> over the same HWCDC port. PLP frames and log text share the wire; the FrameParser
> (both firmware and browser) is noise-tolerant and resyncs on the `0xAB` magic byte.
