# Nema System API — Parity Matrix

> Auto-generated from `api/build/nema-api.json`. Do not edit.
> Host API version: `1.0`

## Coverage

**Total functions:** 38

| Interface | Function | Host C++ | QuickJS | WASM .h | JS .d.ts | Docs | Impl |
|---|---|---|---|---|---|---|---|
| `nema:bt/ble` | `enable` 🔒 | — | — | — | — | ✅ | — |
| `nema:bt/ble` | `disable` | — | — | — | — | ✅ | — |
| `nema:bt/ble` | `is-enabled` | — | — | — | — | ✅ | — |
| `nema:input/input` | `hint` | — | — | — | — | ✅ | — |
| `nema:input/input` | `actions` | — | — | — | — | ✅ | — |
| `nema:media/audio-input` | `list` | — | — | — | — | ✅ | — |
| `nema:media/audio-output` | `list` | — | — | — | — | ✅ | — |
| `nema:media/camera` | `list` | — | — | — | — | ✅ | — |
| `nema:media/camera` | `capture` 🔒 | — | — | — | — | ✅ | — |
| `nema:net/http` | `get` 🔒 | — | — | — | — | ✅ | — |
| `nema:net/http` | `post` 🔒 | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `is-connected` | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `ssid` | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `ip` | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `scan` 🔒 | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `connect` 🔒 | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `disconnect` | — | — | — | — | ✅ | — |
| `nema:profile/profile` | `user-name` | — | — | — | — | ✅ | — |
| `nema:profile/profile` | `device-name` | — | — | — | — | ✅ | — |
| `nema:profile/profile` | `has-password` | — | — | — | — | ✅ | — |
| `nema:profile/profile` | `verify-password` | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `get` | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `set` | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `get-int` | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `set-int` | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `remove` | — | — | — | — | ✅ | — |
| `nema:sys/log` | `log` | — | — | — | — | ✅ | — |
| `nema:sys/device` | `name` | — | — | — | — | ✅ | — |
| `nema:sys/device` | `caps` | — | — | — | — | ✅ | — |
| `nema:sys/device` | `has` | — | — | — | — | ✅ | — |
| `nema:sys/device` | `available` | — | — | — | — | ✅ | — |
| `nema:sys/events` | `subscribe` | — | — | — | — | ✅ | — |
| `nema:sys/events` | `unsubscribe` | — | — | — | — | ✅ | — |
| `nema:sys/events` | `publish` | — | — | — | — | ✅ | — |
| `nema:sys/tasks` | `submit` | — | — | — | — | ✅ | — |
| `nema:sys/tasks` | `timeout` | — | — | — | — | ✅ | — |
| `nema:sys/tasks` | `interval` | — | — | — | — | ✅ | — |
| `nema:sys/tasks` | `cancel` | — | — | — | — | ✅ | — |

## Legend

| Symbol | Meaning |
|---|---|
| ✅ | Generated / implemented |
| — | Not yet generated (pending phase) |
| 🔒 | `@blocking` — must run on TaskRunner worker |