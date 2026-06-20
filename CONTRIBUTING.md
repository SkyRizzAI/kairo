# Contributing to Palanu

Thanks for your interest in Palanu! This is a hardware-agnostic embedded firmware
runtime — one core, many boards. Contributions of all kinds are welcome: new
boards and drivers, apps, core features, bug fixes, docs, and tests.

By contributing, you agree that your contributions are licensed under the
project's [GPLv3 license](LICENSE).

---

## Getting started

You need [Bun](https://bun.sh) (for the toolchain/scripts) and CMake + a C++17
compiler (for the host build and tests). For on-device builds you also need the
[ESP-IDF](https://docs.espressif.com/projects/esp-idf/) toolchain, and for the
WASM simulator you need [Emscripten](https://emscripten.org).

```bash
bun install

# Host build + run the full test suite (no hardware needed)
bun test

# Browser simulator (WASM + Forge UI)
bun run forge:wasm

# Real hardware
bun run build:skyrizz-e32   &&  bun run flash:skyrizz-e32
bun run build:dev-board     &&  bun run flash:dev-board
```

`bun test` configures `firmware/build`, compiles, and runs `ctest`. Every PR
must keep this green.

---

## Architecture & layering

Palanu is strictly layered — each layer may only depend on the one below it:

```
targets/    buildable apps   (skyrizz-e32, dev-board, wasm, …)
boards/     drivers + IBoard (dev-board, skyrizz-e32, simulator)
platforms/  OS glue          (esp32, wasm)
core/       hardware-agnostic runtime — no hardware deps
```

A new device is **a folder of drivers + a pin map**, not a fork. To bring up a
board you add `firmware/boards/<name>/` (drivers + `IBoard`) and a
`firmware/targets/<name>/` (the buildable project). The core, apps, settings
screens, and UI come for free.

---

## Project conventions (please read [CLAUDE.md](CLAUDE.md))

These rules apply to **every board and every layer** — they are enforced in
review:

- **Logging** — all logging goes through the Palanu Logger (`rt.log()`). Never use
  raw `Serial`, `printf`/`std::cout`, or `ESP_LOGx`. Use a stable component tag
  and structured fields: `rt.log().info("Xl9535", "started", {{"addr", "0x20"}})`.
- **Hardware abstraction** — check capabilities, never board type. Use
  `rt.capabilities().has(caps::NetWifi)`, not `#ifdef ESP32` or board-name
  branches. Capabilities are a two-axis model (Plan 42):
  - Identity is a **string from the catalog** `nema/system/capabilities.h`
    (`caps::Display`, `caps::Camera`, …) — never a raw string literal.
  - `has(cap)` → **static**: "this box was built able to do X" (never changes).
    Use it for boot gates and for showing/hiding a settings entry (the WiFi
    entry should appear even when disconnected, so the user can connect).
  - `available(cap)` → **dynamic**: "X is up and usable *right now*". Use it
    when a resource can detach/fault (remote screen, storage, network).
  - Inside running code, prefer `container().resolve<IFoo>()` to get the driver
    and act on it — don't double-gate with `has()` *and* `resolve()`.
  - The owner of a resource reports liveness via
    `capabilities().setState(cap, ResourceState::…)`, which publishes
    `events::ResourceChanged`. **`setState` is main-thread-only** (it touches the
    single-threaded EventBus); from a background driver callback, route the
    change through the async event path instead.
- **Input** — screens program against `input::Action` (Prev/Next/Activate/Back),
  not raw physical keys. Each board provides one `IKeyMap` that passes
  `validateFloor()`. Footer labels come from `rt.input().hintFor(Action)`.
- **UI** — resolution-independent: draw from `canvas.width()`/`canvas.height()`,
  never hardcode screen dimensions.
- **Pin maps** are the single source of truth in the board's `board_config.h` —
  no magic pin numbers scattered through drivers.

## Code style

C++17, formatted with `clang-format` using the repo's [`.clang-format`](.clang-format):

```bash
# Format files you touched before committing
clang-format -i path/to/file.cpp
```

4-space indent, K&R braces, left-aligned pointers (`Foo* x`). Match the
surrounding code's naming and comment density.

---

## Pull requests

1. Fork and branch from `main` (e.g. `feat/...`, `fix/...`, `chore/...`).
2. Keep changes focused; one logical change per PR.
3. Run `bun test` — it must pass. Add tests for new core behavior where practical.
4. Use [Conventional Commits](https://www.conventionalcommits.org) for commit
   and PR titles (`feat:`, `fix:`, `docs:`, `chore:`, …) — releases and the
   changelog are automated from these via Release Please.
5. Fill in the PR template and describe what you changed and how you verified it.

## Reporting bugs & requesting features

Open an issue using the templates. For security issues, **do not** open a public
issue — see [SECURITY.md](SECURITY.md).
