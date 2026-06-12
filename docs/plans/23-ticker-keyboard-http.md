# 23 — HTTP HAL, Virtual Keyboard, Ticker App

> Networked-app + text-entry foundation on the Nema kernel (19.5/19.6) and WiFi (20). Adds a platform-agnostic HTTP client, a reusable on-screen QWERTY keyboard, and the flagship BTC/USD Ticker that proves non-blocking networking end-to-end.

- Status: ✅ Implemented & build-green (host + ESP32). Sim verified: Ticker fetched live BTC (HTTP 200, UI 42 frames during fetch = never frozen); WiFi grid-keyboard connect flow works (scan+connect, 0 JSON corruption). HW verification pending.
- Milestone: M7 (Connectivity)
- Depends on: 19.5 (TaskRunner), 19.6 (app-model), 20 (WiFi)

---

## 1. IHttpClient HAL (`hal/http_client.h`)
Blocking `get(url, insecure)` → `HttpResponse{status, body}`. **MUST run on a TaskRunner worker** (never the UI/app loop). Platform impls:
- **Sim** (`SimHttpClient`): shells out to `curl` via `popen()` → real network in the simulator (honest demo, verified: live Binance 200).
- **ESP32** (`Esp32HttpClient`): `esp_http_client` + event-handler body collection (8KB cap), verified TLS via `esp_crt_bundle_attach` (same as reference firmware). `insecure` toggles common-name check. Registered via `onRegister()` lifecycle; adds `http` capability. CMake REQUIRES `esp_http_client esp-tls mbedtls`.

## 2. VirtualKeyboard (`ui/virtual_keyboard.h`)
On-screen QWERTY grid driven by 6 buttons (mirrors the reference firmware's keyboard, now a Palanu core API). Pure state + draw, rendered fullscreen inside the **calling app's own buffer** — no ViewDispatcher, app-model safe. D-pad moves cursor, Select types (`<`=backspace, `OK`=submit), Cancel aborts; `mask` for passwords. DRY: any app reuses it. (Linear `TextInput` retained but WifiApp now uses the grid.)

## 3. TickerApp (`apps/ticker_app.h`)
BTC/USD from Binance (`api.binance.com/api/v3/ticker/price?symbol=BTCUSDT`). Fetch runs on a worker; the app thread animates a spinner ("BTC/USD | (UI alive)") and stays cancelable throughout. **The reference firmware's ticker comment says it BLOCKS the UI during fetch; ours does not** — the whole point of the Nema kernel, shown end-to-end. Auto-fetches on open, OK to refresh. Launched via `TickerPlugin` (own thread).

## 4. Integration
- `WifiApp` password entry: linear picker → `VirtualKeyboard`.
- `TickerPlugin` registered in both sim + dev-board launchers.
- HTTP client registered + `http` capability in both platforms.

## 5. Verification
- Sim: `SimHttpClient GET status=200 bytes=47` (live Binance) on worker; **42 UI frames during the fetch** = UI alive.
- Sim: WiFi scan → pick secured → grid keyboard → submit → `NetworkConnected`; 33 frames, 0 JSON corruption.
- ESP32: `palanu-dev-board.bin` builds, 64% free.

## 6. Pending / Non-goals
- HW flash verification (Ticker over real WiFi; keyboard on e-ink).
- Keyboard: caps lock / extended symbols — future.
- Ticker: multiple symbols, 24h stats, history chart — future (ref firmware has 24h stats).
