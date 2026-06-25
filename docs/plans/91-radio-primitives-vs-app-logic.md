# Plan 91 — Radio primitives vs app logic (kernel/app separation)

Status: Stage 1 + Stage 2 COMPLETE. The kernel WiFi API is now pure generic
mechanism (scan / monitor / inject / setMac / apStart / sockets) — zero attack or
captive-portal verbs. Attacks AND the evil portal live entirely in the app.
`grep -niE "evilPortal|deauthStart|beaconSpam|karmaStart|wait_event|epRun"
firmware/core firmware/platforms` → 0 (excl build/generated). Stage 3 (move
arpScan/tcpProbe to a network-tools app) remains, optional.

## Progress

- ✅ **App owns the attacks** — `examples/wifi-marauder/main.c` builds the deauth /
  probe / beacon / rickroll / karma frames and injects them in its own
  `ui_poll_event` loop, using only `wifi_inject` + `wifi_monitor_read`. Scripts
  runner converted to self-contained bursts.
- ✅ **esp32 kernel stripped** — removed `deauthStart/Stop`, `beaconSpamStart/Stop`,
  `probeFloodStart/Stop`, `karmaStart/Stop`, all four `*Loop` bodies,
  `handleKarmaFrame`, the six frame templates, the `do*` flags + members, and the
  whole `RadioLoop` thread (`loopThread_`) from `Esp32WifiRadio`. `grep deauth|
  beacon|probe|karma firmware/platforms/esp32/src/esp32_wifi_radio.cpp` → 0 logic
  refs. Frees the RadioLoop's 8 KB PSRAM stack too.
- ✅ **WASM host bindings removed** — `wifi_deauth_*`, `wifi_beacon_spam_*`,
  `wifi_probe_flood_*`, `wifi_karma_*` deleted from `wasm_wifi.cpp` + `nema_api.h`.
  Apps can no longer call attack verbs; they must compose from primitives.
- ✅ **Vestigial cleanup DONE** — removed the attack methods from the `IRadioWifi`
  HAL base (`radio_wifi.h`), from the IDL (`api/wifi.pidl`) + regenerated host API
  / quickjs bindings / SDK (`generated/` + `host/` + `sdk/`), from the JS host
  (`nema_host_impl.cpp`), and from the **simulator** (`SimWifiRadio` — dropped its
  deauth/beacon sim loops, kept only the monitor-frame generator). Verified:
  `grep deauthStart|beaconSpamStart|… firmware` (excl build/generated) → **0 refs**.
  esp32 firmware builds clean.
- ✅ **Stage 2 Phase 1 — primitives + app rewrite** (builds): added generic
  `wifi_ap_start/stop` (HAL + esp32, the EXACT working AP sequence) and a generic
  socket layer — `INetSockets` HAL (`net_sockets.h`), `Esp32NetSockets` (lwip),
  host bindings `net_udp_*`/`net_tcp_*` + SDK decls. The app's evil portal is now
  built entirely in `main.c`: `wifi_ap_start` + UDP:53 DNS catch-all + TCP:80 HTTP
  server, polled from its UI loop. The app no longer calls `wifi_evil_portal_*`.
- ✅ **Stage 2 Phase 2 — kernel evil portal stripped** (builds clean): removed
  `evilPortalStart/Stop`, `epRun`, `epThreadEntry`, `epDnsReply`, `kPortalPage`,
  and `epThread_/epRunning_/epSsid_/epHtml_` from `Esp32WifiRadio`; removed
  `evilPortalStart/Stop` + the whole `wait_event`/`pushEvent`/`eventQ_` matured-
  event ring from the `IRadioWifi` HAL; removed `wait-event` from `api/wifi.pidl`
  (regenerated `generated/` + `host/`), the `radio_wait_event` override from the
  JS host, the `wifi_evil_portal_*` + `wifi_wait_event` host bindings from
  `wasm_wifi.cpp`, and their SDK decls from `nema_api.h`. Simulator `inject()` no
  longer pushes events. The `AppHostExited` handler now restores STA if an app
  left a soft-AP up (e.g. a crashed portal). DHCP DNS option added to `apStart`
  so client captive checks resolve to the portal.

## Principle (the test every kernel↔app seam must pass)

> If a custom app is **not installed**, is the kernel code still useful?
> If NO → that code is **app logic** and must live in the app, not the kernel.

The kernel exposes **generic mechanism**; the app supplies **policy + content**.
The kernel must never know what "deauth", "beacon", "karma", or "evil portal"
mean — those are WiFi Marauder concepts. A device with no Marauder installed
should carry zero deauth/portal code.

## Violations today (firmware knows app concepts)

`IRadioWifi` / `Esp32WifiRadio` / `wasm_wifi.cpp` contain app-specific logic:

| Firmware code (app-specific) | Why it's wrong |
|---|---|
| `deauthStart/Stop` + `deauthLoop` | builds a deauth frame, loops inject — "deauth" is app policy |
| `beaconSpamStart/Stop` + `beaconLoop` | builds fake beacons — app content |
| `probeFloodStart/Stop` + `probeLoop` | builds probe frames — app content |
| `karmaStart/Stop` + `handleKarmaFrame` | probe-response attack logic — app policy |
| `evilPortalStart/Stop` + `epRun` + DNS/HTTP server + `kPortalPage` HTML | a whole captive-portal app baked into the kernel |
| `wifi_wait_event` | only exists to drain the firmware loops' events |

## Target API (generic primitives only)

Kernel keeps / adds, exposed to the WASM runtime:

| Primitive | Purpose |
|---|---|
| `wifi_acquire` / `wifi_release` | radio takeover lease (already generic) |
| `wifi_scan(out,cap)` | passive AP scan |
| `wifi_inject(ch, frame, len)` | send ONE raw 802.11 frame |
| `wifi_monitor_open/read/close` | promiscuous capture |
| `wifi_set_mac(mac)` | radio MAC |
| `wifi_ap_start(ssid, ch, open)` / `wifi_ap_stop()` | **new** — generic soft AP |
| `net_udp_*` / `net_tcp_*` (bind/recv/send/listen/accept/close) | **new** — raw sockets so an app can build DNS/HTTP itself |

The app builds frames and runs loops itself, pacing via `ui_poll_event` so the UI
stays responsive (deauth/beacon/probe at ~10–20 Hz is well within a poll loop).
No firmware thread needs to know the frame's purpose.

## Migration stages

**Stage 1 — loops → app (no new primitives needed).** Remove deauth / beacon /
probe / karma from `IRadioWifi`, `Esp32WifiRadio`, the simulator radio, and
`wasm_wifi.cpp`. Re-implement each in `examples/wifi-marauder/main.c` using
`wifi_inject` + `wifi_monitor_read` in the screen's poll loop (the app already
builds frames in `inject_badmsg`/`inject_sleep`). Drop `wifi_wait_event`.

**Stage 2 — evil portal → app.** Add generic `wifi_ap_start/stop` + a raw socket
host API. Move the DNS-catch-all + HTTP captive server + HTML into the app. Then
delete `evilPortalStart/Stop`/`epRun`/`kPortalPage` from the kernel.

**Stage 3 — network tools.** `staStatus` stays (generic). `arpScan`/`tcpProbe`
move to the dedicated network-tools app once sockets exist (Stage 2 API).

## Done when

`grep -rniE "deauth|beacon|karma|evil|portal" firmware/` returns **nothing** in
core/platforms (only app code references these). The kernel radio API is a list
of verbs that describe *mechanism*, never *attack*.
