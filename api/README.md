# Nema System API IDL — Phase 1 Freeze

> SSOT permukaan API non-UI Palanu (Plan 48). WIT-subset → JSON AST → generator (Plan 49).

## Daftar interface

| Interface ID | File | Status | Capability | v0 parity (js_api.cpp) |
|---|---|---|---|---|
| `nema:sys/log` | `sys.pidl` | core | — | `api_log` (line 29) |
| `nema:sys/device` | `sys.pidl` | core | — | `api_has`+`api_available`+`dev.name`+`dev.caps` (line 42–51, 128–136) |
| `nema:sys/events` | `sys.pidl` | core / @future | — | not yet in v0 |
| `nema:sys/tasks` | `sys.pidl` | core / @future | — | not yet in v0 |
| `nema:storage/kv` | `storage.pidl` | core | — | `api_store_get/set/remove` (line 54–70) |
| `nema:net/http` | `net.pidl` | gated | `net.http` | `api_http_get` (line 96–105) |
| `nema:net/wifi` | `net.pidl` | gated / @future | `net.wifi` | not yet in v0 |
| `nema:profile` | `profile.pidl` | gated | `profile` | `api_profile_*` (line 74–93) |

## Not yet authored (future phases)

Interfaces from Plan 48 §3 that need `.pidl` files but are out of scope for Phase 1
(because they have no JS v0 binding to verify parity against):

| Interface ID | Planned file | Capability |
|---|---|---|
| `nema:storage/fs` | `api/storage.pidl` (add `fs` interface) | `storage` |
| `nema:bt/ble` | `api/bt.pidl` | `bt.ble` |
| `nema:input` | `api/input.pidl` | `input` |
| `nema:media/audio` | `api/media.pidl` | `audio.output` / `audio.input` |
| `nema:media/camera` | `api/media.pidl` | `camera` |
| `nema:sys/power` | `api/sys.pidl` (add `power` interface) | core (read) / gated (set) |
| `nema:radio/subghz` | `api/radio.pidl` (future) | `radio.subghz` |

## Version

All interfaces start at `@1.0` (major 1, minor 0). The canonical `NEMA_API_VERSION`
will be generated from the IDL as packed u16.u16 in Phase 3.

## Cross-reference: IDL ↔ js_api.cpp

### nema:sys/log ↔ js_api.cpp:29

| IDL | js_api.cpp |
|---|---|
| `log(level: string, tag: string, msg: string)` | `api_log(ctx, _, argc, argv)` — level→`lvl`, tag→`tag`, msg→`msg` (line 31) |

### nema:sys/device ↔ js_api.cpp:42–51, 128–136

| IDL | js_api.cpp |
|---|---|
| `name() -> string` | `dev.name` = `host_->info().boardName` (line 128) |
| `caps() -> list<string>` | `dev.caps` = `host_->capabilities().list()` (line 129–133) |
| `has(cap) -> bool` | `api_has` → `host_->capabilities().has(cap)` (line 44) |
| `available(cap) -> bool` | `api_available` → `host_->capabilities().available(cap)` (line 50) |

### nema:storage/kv ↔ js_api.cpp:54–70

| IDL | js_api.cpp |
|---|---|
| `get(key) -> option<string>` | `api_store_get` → `config().getString(ns, key, out)` → returns string or JS_NULL (line 54–59) |
| `set(key, value)` | `api_store_set` → `config().setString(ns, key, value)` (line 61–64) |
| `remove(key) -> bool` | `api_store_remove` → `config().remove(ns, key)` → returns JS_UNDEFINED; backend returns bool (line 66–69) |
| `get-int(key) -> option<s64>` | ⚠️ not in v0 (exists in config_store.h:27) |
| `set-int(key, value: s64)` | ⚠️ not in v0 (exists in config_store.h:31) |

### nema:net/http ↔ js_api.cpp:96–105

| IDL | js_api.cpp |
|---|---|
| `get(url) -> result<http-response, string>` | `api_http_get` → `client->get(url)` → `{status, body}` (line 96–105). Error path: throws TypeError if client is null; transport errors yield `status=0`. |
| `post(url, body, content-type) -> result<http-response, string>` | ⚠️ not in v0 (exists in http_client.h — `post` method not yet exposed) |

### nema:profile ↔ js_api.cpp:74–93

| IDL | js_api.cpp |
|---|---|
| `user-name() -> string` | `api_profile_userName` → `p->userName()` (line 74–77) |
| `device-name() -> string` | `api_profile_deviceName` → `p->deviceName()` (line 79–82) |
| `has-password() -> bool` | `api_profile_hasPassword` → `p->hasPassword()` (line 84–87) |
| `verify-password(input) -> bool` | `api_profile_verifyPassword` → `p->verifyPassword(input)` (line 89–92) |

## Naming reconciliation

The IDL uses **canonical names** derived from the Plan 42 capability taxonomy:

| v0 JS path (legacy) | Capability (Plan 42) | IDL canonical path |
|---|---|---|
| `nema.log(level, tag, msg)` | (core) | `nema.sys.log.log(level, tag, msg)` |
| `nema.device.has(cap)` | (core) | `nema.sys.device.has(cap)` |
| `nema.storage.get(key)` | (core) | `nema.storage.kv.get(key)` |
| `nema.http.get(url)` | `net.http` | `nema.net.http.get(url)` |
| `nema.profile.userName()` | `profile` | `nema.profile.user-name()` |

A deprecated alias shim (`nema.http` → `nema.net.http`, etc.) will be emitted by the
generator for one major release (Plan 48 §2, Phase 4).
