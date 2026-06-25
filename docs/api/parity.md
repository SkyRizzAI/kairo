# Nema System API — Parity Matrix

> Auto-generated from `api/build/nema-api.json`. Do not edit.
> Host API version: `1.0`

## Coverage

**Total functions:** 67

| Interface | Function | Cap | Tier | Host C++ | QuickJS | WASM .h | JS .d.ts | Docs | Impl |
|---|---|---|---|---|---|---|---|---|---|
| `aether:ui/view` | `view-begin` | — | — | — | — | — | — | ✅ | — |
| `aether:ui/view` | `view-end` | — | — | — | — | — | — | ✅ | — |
| `aether:ui/text` | `label` | — | — | — | — | — | — | ✅ | — |
| `aether:ui/text` | `styled` | — | — | — | — | — | — | ✅ | — |
| `aether:ui/interactive` | `button` | — | — | — | — | — | — | ✅ | — |
| `aether:ui/scroll` | `scroll-begin` | — | — | — | — | — | — | ✅ | — |
| `aether:ui/scroll` | `scroll-end` | — | — | — | — | — | — | ✅ | — |
| `nema:bt/ble` | `enable` 🔒 | `bt.ble` | — | — | — | — | — | ✅ | — |
| `nema:bt/ble` | `disable` | `bt.ble` | — | — | — | — | — | ✅ | — |
| `nema:bt/ble` | `is-enabled` | `bt.ble` | — | — | — | — | — | ✅ | — |
| `nema:input/input` | `hint` | `input` | — | — | — | — | — | ✅ | — |
| `nema:input/input` | `actions` | `input` | — | — | — | — | — | ✅ | — |
| `nema:media/audio-input` | `list` | `audio.input` | — | — | — | — | — | ✅ | — |
| `nema:media/audio-output` | `list` | `audio.output` | — | — | — | — | — | ✅ | — |
| `nema:media/audio-output` | `set-volume` | `audio.output` | — | — | — | — | — | ✅ | — |
| `nema:media/audio-output` | `play-tone` | `audio.output` | — | — | — | — | — | ✅ | — |
| `nema:media/audio-output` | `play-pcm` | `audio.output` | — | — | — | — | — | ✅ | — |
| `nema:media/camera` | `list` | `camera` | — | — | — | — | — | ✅ | — |
| `nema:media/camera` | `capture` 🔒 | `camera` | — | — | — | — | — | ✅ | — |
| `nema:net/http` | `get` 🔒 | `net.http` | — | — | — | — | — | ✅ | — |
| `nema:net/http` | `post` 🔒 | `net.http` | — | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `is-connected` | `net.wifi` | — | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `ssid` | `net.wifi` | — | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `ip` | `net.wifi` | — | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `scan` 🔒 | `net.wifi` | — | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `connect` 🔒 | `net.wifi` | — | — | — | — | — | ✅ | — |
| `nema:net/wifi` | `disconnect` | `net.wifi` | — | — | — | — | — | ✅ | — |
| `nema:profile/profile` | `user-name` | `profile` | — | — | — | — | — | ✅ | — |
| `nema:profile/profile` | `device-name` | `profile` | — | — | — | — | — | ✅ | — |
| `nema:profile/profile` | `has-password` | `profile` | — | — | — | — | — | ✅ | — |
| `nema:profile/profile` | `verify-password` | `profile` | — | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `get` | — | — | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `set` | — | — | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `get-int` | — | — | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `set-int` | — | — | — | — | — | — | ✅ | — |
| `nema:storage/kv` | `remove` | — | — | — | — | — | — | ✅ | — |
| `nema:storage/fs` | `read-file` | — | — | — | — | — | — | ✅ | — |
| `nema:storage/fs` | `write-file` | — | — | — | — | — | — | ✅ | — |
| `nema:storage/fs` | `list-files` | — | — | — | — | — | — | ✅ | — |
| `nema:storage/fs` | `remove-file` | — | — | — | — | — | — | ✅ | — |
| `nema:storage/fs` | `bytes-used` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/log` | `log` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/device` | `name` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/device` | `caps` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/device` | `has` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/device` | `available` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/events` | `subscribe` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/events` | `unsubscribe` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/events` | `publish` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/perm` | `status` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/perm` | `request` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/lease` | `acquire` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/lease` | `release` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/tasks` | `submit` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/tasks` | `timeout` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/tasks` | `interval` | — | — | — | — | — | — | ✅ | — |
| `nema:sys/tasks` | `cancel` | — | — | — | — | — | — | ✅ | — |
| `nema:wallet/wallet` | `networks` | — | — | — | — | — | — | ✅ | — |
| `nema:wallet/wallet` | `ready` | — | — | — | — | — | — | ✅ | — |
| `nema:wallet/wallet` | `address` | `wallet.read` | sensitive | — | — | — | — | ✅ | — |
| `nema:wallet/wallet` | `sign-message` | `wallet.sign` | sensitive | — | — | — | — | ✅ | — |
| `nema:wallet/wallet` | `sign-transaction` | `wallet.sign` | sensitive | — | — | — | — | ✅ | — |
| `nema:wifi/radio` | `scan` 🔒 | `net.wifi.scan` | benign | — | — | — | — | ✅ | — |
| `nema:wifi/radio` | `monitor-open` 🔑 | `net.wifi.monitor` | sensitive | — | — | — | — | ✅ | — |
| `nema:wifi/radio` | `monitor-read` 🔒 🔑 | `net.wifi.monitor` | sensitive | — | — | — | — | ✅ | — |
| `nema:wifi/radio` | `monitor-close` 🔑 | `net.wifi.monitor` | sensitive | — | — | — | — | ✅ | — |
| `nema:wifi/radio` | `inject` 🔑 | `net.wifi.inject` | sensitive | — | — | — | — | ✅ | — |

## Legend

| Symbol | Meaning |
|---|---|
| ✅ | Generated / implemented |
| — | Not yet generated (pending phase) |
| 🔒 | `@blocking` — must run on TaskRunner worker |
| 🔑 | `@lease` — requires ResourceBroker exclusive grant |