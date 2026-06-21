# Palanu / Kairo — Visual Map

> ASCII-art visualization of the codebase as of **2026-06-21**.
> A companion to [`overview.md`](overview.md), [`STATE.md`](STATE.md) and
> [`architecture/`](architecture/README.md). When code and this doc disagree,
> the code wins.
>
> Palanu = hardware-agnostic embedded firmware runtime (C++17), Flipper-Zero-style
> handheld with a 1-bit retro UI. **One core, many boards** — the same
> `firmware/core/` compiles to ESP32-S3 hardware *and* to a WASM browser simulator.

---

## 1. The four layers (one core, many boards)

```
              ┌───────────────────────────────────────────────────────────┐
   buildable  │  TARGET    firmware/targets/*                              │
   apps       │  one main.cpp: wires a Platform + a Board + a Runtime,     │
              │  installs apps/services, then runs the loop.               │
              │  wasm · dev-board · skyrizz-e32 · skyrizz-{cam,audio}test  │
              ├───────────────────────────────────────────────────────────┤
   physical   │  BOARD     firmware/boards/*            implements IBoard  │
   wiring     │  the hardware wired to the chip: display, buttons,         │
              │  sensors, pin map, keymap, capability profile.            │
              │  simulator · dev-board · skyrizz-e32                       │
              ├───────────────────────────────────────────────────────────┤
   chip / OS  │  PLATFORM  firmware/platforms/*        implements IPlatform│
   environ.   │  clock, WiFi, BLE, USB, NVS, filesystem, OTA, power.       │
              │  esp32  (ESP-IDF v5.5)   ·   wasm  (emscripten + browser)  │
              ├───────────────────────────────────────────────────────────┤
   hardware-  │  CORE      firmware/core/              NO hardware deps    │
   free       │  boot · services · Nema kernel (threads) · UI runtime ·    │
   runtime    │  app model · link/PLP · VFS · HAL interfaces · scripting   │
              └───────────────────────────────────────────────────────────┘
                          ▲                                   ▲
        same core binary ─┘   only platform+board change   ──┘

   Aether (firmware/aether/) = the display server, a SEPARATE lib (Plan 80).
   nema_core has 0 references to aether; swap the server = swap the lib.
```

Dependencies only ever point **downward**: TARGET → BOARD → PLATFORM → CORE.
Core never knows which board or chip it runs on — it asks **capabilities**.

---

## 2. Boot sequence (identical for every target)

```
   main()
     │
     ├─▶ rt.loadPlatform(platform)   register chip/OS drivers (clock, wifi, usb…)
     ├─▶ rt.loadBoard(board)         register hardware (display, keymap, caps)
     ├─▶ rt.initCore()               logger · event bus · services · registries
     ├─▶ rt.registerServices()       input · gui · task-runner · app hosts
     ├─▶ rt.start()                  startAll services → BootPhase::Running
     │
     ├─▶ aether::bootDisplay(rt)     ◀── Plan 80: construct + start display
     │                                   servers + the GUI render loop
     │
     ├─▶ rt.apps().install(...)      DolphinApp, JS apps, ClockService …
     ├─▶ rt.view().push(DesktopScreen)   ◀── Plan 81: idle wallpaper screen
     │
     └─▶ loop:  rt.step()            one tick:  asyncPoster.flush()
                                                serviceManager.tick()
                                                platform.idle()
         (host/wasm: rt.run() blocks · Arduino: loop() calls rt.step())
```

`BootPhase: None → ... → Running`. `rt.step()` is the heartbeat the Arduino
`loop()` (or `emscripten_set_main_loop`) calls forever.

---

## 3. Runtime facade — the kernel's single front door

```
                         ┌──────────────────────────────┐
                         │          nema::Runtime        │  (runtime.h)
                         │   "create() → the whole OS"   │
                         └──────────────────────────────┘
        ┌──────────────┬──────────────┬──────────────┬──────────────┐
        ▼              ▼              ▼              ▼              ▼
  ── identity ──   ── services ──  ── kernel ──   ── apps ──    ── i/o ──
  platform()       input()        tasks()        apps()        canvas()
  board()          audio()        asyncPoster()  appHost()     view()
  clock()          camera()       processes()    config()      displayServer()
  log()            container()                                  fs()
  events()         dpm()  ◀ sleep/lock state machine           cliService()
  capabilities()   serviceState()
  hardware()
  info()

   Everything in the system is reached through `rt`. Apps get a slimmer
   AppContext, but it forwards to the same Runtime underneath.
```

---

## 4. Nema kernel — the "never freeze" thread model

> Nema (νῆμα = "thread"). The architectural pillar: **the UI never blocks.**

```
   CORE 1  (UI)                CORE 0  (work / IO)          main loop (Arduino)
   ───────────                 ──────────────────           ──────────────────
   ┌─────────────────┐         ┌──────────────────┐         ┌────────────────┐
   │ GuiService thr  │         │ TaskRunner worker │         │ rt.step():     │
   │  owns Canvas +  │         │  scan(), http.get │         │  asyncPoster   │
   │  ViewDispatcher │◀──┐     │  (may BLOCK)      │         │     .flush()   │
   │  input→handleKey│   │     └────────┬─────────┘         │  svcMgr.tick() │
   │  draw → flush   │   │              │ done()             │  platform.idle │
   │  task completion│   │   ┌──────────▼─────────┐         └────────────────┘
   └────────┬────────┘   │   │ Input poll thread  │
            │            │   │  (TCA9534 buttons) │
   ┌────────▼────────┐   └───┤  → InputService Q  │
   │ App thread (fg) │       └────────────────────┘
   │  IApp::run()    │
   │  may BLOCK      │   Race-free by design: the ONLY cross-thread state is a
   │  draws own buf  │   pixel buffer (mutex) + queues. Zero shared model.
   │  present()──────┼──▶ handoff to GUI thread. App never touches Canvas direct.
   └─────────────────┘

   Rule: heavy work → tasks().submit(work, done).
         work  runs on a worker thread (may block).
         done  runs on the UI thread (safe to touch state).
```

---

## 5. UI navigation flow (Desktop → Launcher → Apps)

```
   ┌─────────────────────────────────────────────────────────────────────┐
   │  DesktopScreen  (live wallpaper, idle)             Plan 81 / ADR 0004 │
   │  skin ← config "display/desktop"  (livewall …)                       │
   └───────────────────────────────┬─────────────────────────────────────┘
                              OK ▼  (Activate)
   ┌─────────────────────────────────────────────────────────────────────┐
   │  LauncherScreen   skin ← config "display/launcher"                    │
   │  ┌──────────────────────┐   ┌──────────────────────┐                 │
   │  │ PlayStation carousel │   │ Nintendo Wii  2-col   │   swappable     │
   │  │  ◀  ▢  ▢  ▢  ▶        │   │  ▢ ▢   ▢ ▢   ▢ ▢      │   via           │
   │  │  horizontal scrollbar│   │  grid + scrollbar     │   nema::shell   │
   │  └──────────────────────┘   └──────────────────────┘   shell_factory │
   └──────┬────────────────────┬──────────────────────┬─────────────────┘
          ▼                     ▼                      ▼
   ┌─────────────┐      ┌──────────────┐       ┌─────────────────────────┐
   │  Apps       │      │ Files·Dolphin│       │ Settings                │
   │  (AppRegistry      │ Logs         │       │  ├ Display & Appearance  │
   │   .list())  │      └──────────────┘       │  │   Theme/Desktop/      │
   │  ├ Clock    │                             │  │   Launcher/Assets/    │
   │  ├ Counter  │  launch → AppHost spawns    │  │   StatusBar           │
   │  ├ Stopwatch│  the app as its own thread  │  ├ WiFi · Bluetooth      │
   │  ├ TaskDemo │  (IApp::run)                │  ├ Remote · Sleep        │
   │  ├ Ticker   │                             │  ├ Profile · Sounds      │
   │  ├ BadUSB   │                             │  └ About (board/fw/caps) │
   │  └ JS apps  │                             └─────────────────────────┘
   └─────────────┘
```

Screens program against `input::Action` (Prev/Next/Activate/Back/…), never raw
buttons. Footer hints come from `rt.input().hintFor(Action)` — never hardcoded.

---

## 6. Input abstraction — physical button → intent

```
   physical button         board IKeyMap            hardware-agnostic
   + gesture        ──▶    (one per board)    ──▶   intent the app sees
   ───────────────         ─────────────            ────────────────────
   short / long /          translates              enum Action {
   double / chord          Code+gesture →            Prev      = 1  ◀ back/up/left
                           Action, must pass         Next      = 2  ◀ fwd/down/right
   ┌──────────┐            validateFloor()           Activate  = 3  ◀ confirm/enter
   │ Up    ───┼──▶ Prev    (Prev,Next,Activate,      Back      = 4  ◀ escape
   │ Down  ───┼──▶ Next     Back all reachable)      AdjustUp  = 11
   │ Left  ───┼──▶ AdjUp                             AdjustDown= 12
   │ Right ───┼──▶ AdjDn                             Menu      = 13
   │ Enter ───┼──▶ Activate                          Pause     = 14 ◀ app→home
   │ Esc   ───┼──▶ Back    }
   └──────────┘
        │
        ▼  Input poll thread → InputService queue → GUI thread → IScreen::onAction()

   Apps NEVER consume raw Key/Code unless physical identity truly matters.
   Same screen code runs on a 6-button e-ink board AND a touch simulator.
```

---

## 7. Link stack — PLP wire protocol (device ⇄ host)

```
   ┌────────────────────────────────────────────────────────────────────┐
   │  Palanu Forge (browser / CLI)   ── packages/forge, @palanu/link     │
   │  PLP codec in TypeScript  (MUST stay byte-for-byte with firmware)   │
   └───────────────────────────────┬────────────────────────────────────┘
                                   │  same frames, many transports
        ┌──────────────────────────┼──────────────────────────┐
        ▼                          ▼                          ▼
   USB-CDC / JTAG            BLE GATT                   WebSocket (WiFi)
   (HWCDC, Plan 1)          (NimBLE UUIDs)             /plp@8477 (Plan 75)
   wasm virtual cable
        └──────────────────────────┼──────────────────────────┘
                                   ▼
                       ┌────────────────────────┐
                       │   MuxTransport         │  one wire, N channels
                       │   FrameParser (0xAB)   │  resyncs on magic byte
                       └───────────┬────────────┘
                                   ▼
        Frame: [magic 0xAB][chan][flags][len LE][payload][crc8 0x07]

        Channels:  0 Control   1 Screen   2 Input    3 Log     4 System
                   5 Ota       6 Ext      7 Event    8 Cli     9 File

        LinkService routes channels → RemoteService / CliService / OTA / VFS.
        Log text + PLP frames share the same physical wire; parser is noise-tolerant.
```

---

## 8. Display server (Aether) — pluggable rendering backend

```
   CORE  (knows only the neutral contract)          AETHER lib (the backend)
   ──────────────────────────────────────          ─────────────────────────
   ┌───────────────────────────┐                   ┌────────────────────────┐
   │ Runtime                    │  registerDisplay  │ aether::bootDisplay(rt)│
   │  displayServers_: vector<  │◀──────Server()────│  constructs servers +  │
   │     IDisplayServer*>       │                   │  the GuiService loop   │
   │  activeServer_             │                   └───────────┬────────────┘
   │  pendingServer_ (atomic)   │   applyPendingServer()        │
   └───────────┬───────────────┘   (GUI thread: pending→active)│
               │  CLI `display <name>` switches at runtime      ▼
               │                                    ┌────────────────────────┐
   nema_core has ZERO refs to aether                │ fbcon · aether servers │
   (IDF strict-link enforced).                      │ render Canvas → device │
   Swap server = swap lib + bootDisplay().          └────────────────────────┘
```

---

## 9. HAL interface map — core declares, platform/board implement

```
   firmware/core/include/nema/hal/*.h        implemented by
   ─────────────────────────────────         ───────────────────────────────
   IDisplay / AsyncDisplay / BufferDisplay  ▶ board e-ink (GxEPD2) · sim canvas
   IWifi                                    ▶ esp32 (scan/NVS) · sim "router"
   IBluetooth                               ▶ esp32 NimBLE · sim stub
   IHttpClient                              ▶ esp32 (esp_http_client+TLS) · curl
   IUsbCdc / IUsbHid                        ▶ esp32 TinyUSB / HWCDC · n/a
   IFileSystem                              ▶ esp32 flash VFS · host fs · ram
   IOta                                     ▶ esp32 OTA partition · n/a
   ICamera / IAudioIn / IAudioOut           ▶ skyrizz-e32 only
   IBattery                                 ▶ board ADC · DummyBattery fallback
   ISecureElement   (see §11)               ▶ skyrizz SE050 · sim software
   IRemoteScreenTap                         ▶ remote-driven touch injection

   Rule: check rt.capabilities().has("net.wifi"), NEVER #ifdef ESP32 or board name.
```

---

## 10. Capability model — "what can this box do?"

```
   board.describeHardware()  ──declares──▶  CapabilityRegistry
                                                 │
   App / Screen asks ───────────────────────────┤
        has("display")        static  : "built able to do X"   (never changes)
        available("net.wifi") dynamic : "X is up & usable now" (Plan 42 liveness)
                                                 │
   ┌─────────────────────────────────────────────────────────────────────┐
   │ display · input{.prev/.next/.activate/.back/.adjust/.2d/.touch}      │
   │ camera · audio.{input,output} · rgb · sensors.{env,light,motion}     │
   │ net.{wifi,http} · bt.ble{.central} · usb.hid                         │
   │ storage · remote.{usb,net} · profile · secure.element · secure.store │
   └─────────────────────────────────────────────────────────────────────┘
        Gate UI on capability → the same firmware adapts to each board.
```

---

## 11. Secure element — HW root-of-trust (crypto-wallet foundation)

```
   ISecureElement : IDriver           ADR 0005 · 🟡 scaffold (ops TODO)
   ───────────────────────────
   baseline (every chip):  randomBytes() · uniqueId() · genKey(P-256) · sign/verify
   chip-specific (probe!):  supportsKeyType() · hasFeature()

       SeKeyType                         backends (caps::Secure)
       ─────────                         ───────────────────────
       EccP256    ✔ guaranteed baseline   ┌─ skyrizz-e32: SE050 (I²C driver)
       Secp256k1  ⚠ BTC/EVM  SE050 only   ├─ simulator : software sim
       Ed25519    ⚠ Solana   SE050E only  └─ board w/o chip → caps off, SW fallback

   Private keys are generated INSIDE the chip and never leave it — the whole point.
   Apps gate on caps::Secure, resolve<ISecureElement>(), never branch on chip type.
```

---

## 12. App model — every app is its own thread

```
   AppRegistry (install/list)        AppHostManager (launch/pause/resume)
   ───────────────────────────       ─────────────────────────────────────
   AppManifest{id,name,icon,factory} ──launch──▶ spawn thread → IApp::run()
        │                                              │
        │  built-in:  Clock·Counter·Stopwatch·         │  AppContext (slim
        │             TaskDemo·Ticker·Dolphin·BadUSB    │   Runtime facade)
        │  scripted:  JS .papp  (QuickJS)               │
        │  headless:  services  (ClockService …)        ▼
        └──────────────────────────────────────▶  draws to own buffer
                                                   present() → GUI composites
                                                   (Normal: +status bar /
                                                    Fullscreen: app owns screen)

   Scripting runtimes:  QuickJS (UI apps, .papp)  ·  wasm3 (headless modules)
```

---

## 13. Build targets matrix

```
   target            platform   board         output                  status
   ───────────────   ────────   ───────────   ─────────────────────   ──────
   wasm              wasm       simulator     firmware.wasm (browser)  ✅ sim
   dev-board         esp32      dev-board     palanu-dev-board.bin     ✅ HW
   skyrizz-e32       esp32      skyrizz-e32   skyrizz-e32.bin          ✅ build
   skyrizz-camtest   esp32      skyrizz-e32   camera bring-up          test
   skyrizz-audiotest esp32      skyrizz-e32   mic/speaker bring-up     test

   bun run forge:wasm     core C++ → WASM → Forge (/simulator)
   bun run build:esp32    → build/palanu-dev-board.bin  (~1.3 MB, 59% free)
   bun run flash:esp32    flash + serial monitor
   bun run test           host unit tests (layout / PLP / link) via ctest

   SkyRizz E32 USB modes (toggle 2 lines in target CMakeLists, then clean build):
     HID/CDC mode → BadUSB + Forge over USB CDC (TinyUSB)   needs download mode
     JTAG/Serial  → fast flashing + Forge over HWCDC        ◀ current state
```

---

## 14. Repo map (where things live)

```
   firmware/
   ├─ core/                    hardware-free runtime (the OS)
   │   ├─ include/nema/        public headers
   │   │   ├─ runtime.h        the facade · thread.h · task_runner.h · message_queue.h
   │   │   ├─ hal/             interface contracts (display,wifi,ble,http,se,fs…)
   │   │   ├─ input/           action·code·keymap·gesture·pointer
   │   │   ├─ link/            plp_codec · transports · mux · link_service
   │   │   ├─ screens/         desktop·launcher·settings·wifi·about·logs…
   │   │   ├─ shell/           desktop/launcher SKINS + shell_factory (Plan 81)
   │   │   ├─ app/ apps/       app-model + built-in apps
   │   │   ├─ services/ system/ proc/ event/ fs/ crypto/ js/ wasm/ config/
   │   └─ src/                 implementations (mirror of include)
   ├─ aether/                  display server lib (Plan 80) — boot_display.cpp
   ├─ servers/fbcon/           framebuffer console server backend
   ├─ platforms/  esp32 · wasm
   ├─ boards/     simulator · dev-board · skyrizz-e32
   ├─ targets/    wasm · dev-board · skyrizz-e32 · skyrizz-{cam,audio}test
   ├─ vendor/     wasm3 · quickjs · arduino-libs · nlohmann
   └─ tests/      host unit tests (layout / KLP / link)

   packages/forge/   SvelteKit web client: /simulator (WASM) · /remote · /flash
   docs/             STATE · overview · architecture/ · plans/ · decisions/ · feats/
```

---

### Legend

```
   ✅ done/verified   🟡 scaffold (ops TODO)   ⚠ chip-specific/caution   ◀ note
   ──▶ data/control flow      │ ▼ ▲ ◀ structure/nesting
```

> Diagrams reflect the state on branch `feat/plan81-desktop-launcher-shell`.
> Update alongside the code (CLAUDE.md "Documentation — automated upkeep").
</content>
</invoke>
