# CLI Terminal

> Plans 40 / 44 / 45 / 57 — A command shell over the PLP Cli channel, with independent
> per-connection sessions, a cwd over the VFS, and `PATH`-based app auto-launch.

## Overview

The device runs a command **registry** (not a Unix kernel) reachable from Forge's terminal panel.
Each browser connection gets its own `CliSession` (own cwd, history, output sink). Built-in commands
introspect the live system; custom apps on the `PATH` can be launched by name.

## How it works

```
Forge CliTerminal ── PLP Cli [sid][command line] ──▶ RemoteService.dispatch
                                                       └─ sessions_->get(sid)
                                                          cli_->execute(line, session)
                                                          ◀── output frames
                                                          ◀── prompt update [0x01]<cwd>
                                                          ◀── EOT [sid][0x04]
```

- **Per-session state** (`CliSession`): cwd over the VFS, capped history ring, output sink,
  `lastExit`, and a `PATH` (`/apps`, `/sd/apps`) scanned for app auto-launch.
- **Session manager** keys sessions by a 1-byte id with stable storage so a `CliSession&` stays
  valid; all sessions are cleared on disconnect.

## Built-in commands

`registerCoreCliCommands(cli, rt)` installs (reading everything through `rt`):
`help`, `hwinfo`, `ram` (ESP32: live heap/PSRAM), `caps`, `display`, `power`, `wlan`/`network`,
`ble`, `whoami`, `profile`, `fs`, `pwd`/`cd`, `history`. Platforms specialize (ESP32 replaces `ram`
with a live-heap version).

## File reference

| File | Purpose |
|---|---|
| `firmware/core/include/nema/services/cli_service.h` / `src/services/cli_service.cpp` | Registry, `CliSession`, `CliSessionManager`, core commands |
| `firmware/core/src/services/remote_service.cpp` | Cli channel dispatch (sid, EOT, prompt) |
| `packages/forge/src/lib/components/CliTerminal.svelte` | Host terminal UI |

## Usage

Connect in Forge (Remote or Simulator) → open the **CLI** panel. Type `help` to list commands.
`cd`/`pwd` navigate the VFS; running an app name found on the `PATH` launches it.

## Extending

- Add a command: `cli_.add("name", "help text", [](CliContext& c){ ... c.out("..."); });` — `c.args`
  is the parsed argv, `c.session` is the calling session (cwd/history/exit).
- App auto-launch scans the session `PATH`; install custom apps under `/apps` (see
  [`custom-apps.md`](custom-apps.md)).
