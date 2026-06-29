# Changelog

## [1.4.0](https://github.com/SkyRizzAI/kairo/compare/palanu-v1.3.0...palanu-v1.4.0) (2026-06-29)


### Features

* **app-platform:** Plan 89 — resource lease, async storage, SD card info (Fase 2–4) ([2a6f7e8](https://github.com/SkyRizzAI/kairo/commit/2a6f7e829f38e331912c3aadbeda0cec087d7049))
* init ble for esp32-s3 ([35622f4](https://github.com/SkyRizzAI/kairo/commit/35622f4642cc07bb1e84c8b97070246650872b66))
* initialize secure element & wallet ([85c4c06](https://github.com/SkyRizzAI/kairo/commit/85c4c06318effb16efd05a6d0d54f78eb71d8b65))
* more consistency ui and ux ([cf2a928](https://github.com/SkyRizzAI/kairo/commit/cf2a9284aa872456e14a336ee7a91853fb0bb927))
* **sim:** default simulator panel to 320×240 @ 2× UI scale ([f66f86b](https://github.com/SkyRizzAI/kairo/commit/f66f86b5a895452cff24cea04155c37ba8d843fe))
* skyrizz-solana board, web3 test app, full control request api ([cb8bc03](https://github.com/SkyRizzAI/kairo/commit/cb8bc0339be594214993420d66cb9c10b5e1be95))
* sub folder apps support ([62b37a3](https://github.com/SkyRizzAI/kairo/commit/62b37a33da3220c087355cc5998f3008c9d5745f))
* **ui:** display rotation, colour themes, Mission Control, splash + radio WIP ([214d487](https://github.com/SkyRizzAI/kairo/commit/214d48796cb2c51f5220b8c2e3204016936335a6))
* **ui:** Plan 90 F4.1/F4.2 — screen transitions + AnimatedValue ([62790d3](https://github.com/SkyRizzAI/kairo/commit/62790d35c9d2a2cbe51696f9287d6c86442d666e))
* **ui:** Plan 90 F6.05 — FileBrowserScreen VirtualList migration ([5da062a](https://github.com/SkyRizzAI/kairo/commit/5da062a309885123c5ea71b7e0badf12a2d1bacc))
* **ui:** Plan 90 F6.08/F6.11 — BadUSB VirtualList + storage capacity bars ([aa624d2](https://github.com/SkyRizzAI/kairo/commit/aa624d2c728ec540a23e07a495be74899abc4e32))
* **ui:** Plan 90 F6.15 + VirtualList selfHighlight + plan audit ([501d5a7](https://github.com/SkyRizzAI/kairo/commit/501d5a7001df9a8358c0c11414380441aae4a318))
* **ui:** Plan 90 F6.16/F6.29 — dynamic footer hints in Dolphin screens ([bdfafd6](https://github.com/SkyRizzAI/kairo/commit/bdfafd6a29ffcb1f71be89d89fca21eee069b6f1))
* **ui:** Plan 90 F6.27/F6.28 — TextViewer line count + HelloApp modernize ([5f4e7f5](https://github.com/SkyRizzAI/kairo/commit/5f4e7f5a22d5d4a10865f8bdd17ff01e12cf5c90))
* **ui:** Plan 90 F6.B4 — FileBrowserScreen breadcrumb header ([6a2a5f6](https://github.com/SkyRizzAI/kairo/commit/6a2a5f6b0bc218d5a4b2da80f7b3445cfe844bb3))
* **ui:** Plan 90 Fase 1+2+3 — Aether UI maturity ([7b4cdbf](https://github.com/SkyRizzAI/kairo/commit/7b4cdbfeff339976d7b69fd1df7668b3c12c2790))
* **ui:** Plan 90 Fase 5+6 — Context API, NodeRef, VirtualList, dialog polish ([0779900](https://github.com/SkyRizzAI/kairo/commit/0779900939d9b608fc1fd7fdbdf9c86575dd5bc4))
* **wasm:** wire PermissionService::request() into all WiFi bindings ([4ea93ac](https://github.com/SkyRizzAI/kairo/commit/4ea93ac1db139047aa8dc465335ab66974d82d14))
* **wifi-marauder:** add probe flood, sniff modes, signal monitor, rickroll ([6f4929d](https://github.com/SkyRizzAI/kairo/commit/6f4929dd4fabb781faf926a964b391f1bf024df3))


### Bug Fixes

* **esp32:** add aether as cmake component dependency ([6c5b821](https://github.com/SkyRizzAI/kairo/commit/6c5b821dcfbe77ca184b79b254afbdce96d97dac))
* **misc:** WiFi radio driver, WASM storage isolation, cp path fix ([05dfc40](https://github.com/SkyRizzAI/kairo/commit/05dfc40b8ca3d9ba8bfa102e266c5ee9cd095f35))
* **permission:** Back=skip-not-deny, two-way toggle, net.wifi.scan in cap list ([d7a0eb5](https://github.com/SkyRizzAI/kairo/commit/d7a0eb512c4825f1c317fe03fc238c57987a79fc))
* **radio:** auto-stop monitor/inject mode on app exit (WiFi lifecycle gap) ([e9914cb](https://github.com/SkyRizzAI/kairo/commit/e9914cb8c31441ef5d87c7265a378b7d3aecf784))
* secure element in skyrizz-e32 ([4d2d611](https://github.com/SkyRizzAI/kairo/commit/4d2d611fec68782cd573a78c7174da4121a34b3a))
* **ui:** 3 bugs — breadcrumb overflow, BadUSB in app list, /sd/apps not scanned ([558670c](https://github.com/SkyRizzAI/kairo/commit/558670c559ec0cf913b49b36ae2cd0252637b591))
* **ui:** dangling ptr in AppDetailScreen + stale render + WASM button width ([a40323e](https://github.com/SkyRizzAI/kairo/commit/a40323e1a70df871b78e587e06f16fbe76cc24c8))
* **ui:** permission modal layout + WiFi toggle crash after monitor-mode app ([b02d7b5](https://github.com/SkyRizzAI/kairo/commit/b02d7b57dffc64193d6b6c32804465b79bee0d1a))
* **ui:** restore apps list + fix VirtualList full-width highlight + CLI size formatter ([d0c1fdc](https://github.com/SkyRizzAI/kairo/commit/d0c1fdc9a57b7b1ebc9bea97bff8c2324230f033))
* **ui:** VirtualList marquee + Left/Right nav parity with settings ([741543a](https://github.com/SkyRizzAI/kairo/commit/741543a683737aa35030607a05ddf880236534a6))
* wallet se first, fallback to software key mode ([4879473](https://github.com/SkyRizzAI/kairo/commit/48794738b28d8999d9e805d8b644b0461e434184))

## [1.3.0](https://github.com/SkyRizzAI/kairo/compare/palanu-v1.2.0...palanu-v1.3.0) (2026-06-23)


### Features

* **fonts:** add ProFont pack (Flipper Zero keyboard/terminal font) ([b4a6f13](https://github.com/SkyRizzAI/kairo/commit/b4a6f13737d1d5470d72ae3d133c737756f236ba))


### Bug Fixes

* **nvs:** call start() explicitly + lazy-init worker to fix PSRAM-stack crash ([3f7ed72](https://github.com/SkyRizzAI/kairo/commit/3f7ed72cc80bec2f7d5d84c013db64a61f677f09))
* **nvs:** proxy all NVS ops (reads + writes) to internal-SRAM worker task ([56b9ea8](https://github.com/SkyRizzAI/kairo/commit/56b9ea8ef5bd8441097bb06f4fd94657442fa845))
* ram usage, remote protokol, forge cli ([98114b6](https://github.com/SkyRizzAI/kairo/commit/98114b681c829a2c7ee4aabac8c50de8d526b429))
* **wasm:** use PSRAM stack (128KB) for nema_app thread on ESP32 ([ce0117b](https://github.com/SkyRizzAI/kairo/commit/ce0117b5353e2bb072ad99f2098515e21ec3ac19))

## [1.2.0](https://github.com/SkyRizzAI/kairo/compare/palanu-v1.1.0...palanu-v1.2.0) (2026-06-22)


### Features

* **app:** WiFi Marauder WASM + wifi.* host bindings (plan 87 fase 8) ([a807aba](https://github.com/SkyRizzAI/kairo/commit/a807abad3e16013cbf1b8149671ed491e0df4c71))
* **broker:** system WiFi coordination + exclusivity groups (plan 87 fase 3) ([bc3a670](https://github.com/SkyRizzAI/kairo/commit/bc3a670a7b39fc6f0dd58ea13bf7c5f5463cad4c))
* **fonts:** add IBM Plex Mono font pack ([a8b7c44](https://github.com/SkyRizzAI/kairo/commit/a8b7c444800523c14141ef0aaa53deb5518c7653))
* **plan87/fase0:** IDL @capability/@tier/[@lease](https://github.com/lease) annotations + gating codegen ([d17d8ee](https://github.com/SkyRizzAI/kairo/commit/d17d8eece369a0058a51fb4ae4d9c0b238ed3e38))
* **plan87/fase1:** PermissionService + PermissionScreen + perm.request/status wiring ([11d7704](https://github.com/SkyRizzAI/kairo/commit/11d77049d51b40d8c0ed5bffd62684a7e519d4d1))
* **plan87/fase2:** ResourceBroker — exclusive HW leases + auto-release on exit ([a1f3171](https://github.com/SkyRizzAI/kairo/commit/a1f3171b2c1bd79af68ad2f42831e9dab8030460))
* **radio:** IRadioWifi HAL + SimWifiRadio + Esp32WifiRadio (plan 87 fase 4) ([e0ffdfe](https://github.com/SkyRizzAI/kairo/commit/e0ffdfe2eda279e932110d400ee6172436dd9d40))
* **radio:** monitor mode + raw inject — Fase 5 (plan 87) ([9229999](https://github.com/SkyRizzAI/kairo/commit/9229999c41546ad1cfba649ae2eff7f71b58ec6b))
* **settings:** App detail screen + permission revoke (plan 87 fase 7) ([817a926](https://github.com/SkyRizzAI/kairo/commit/817a926d50e9e8f56b21bb1f81da3123caa780e8))
* **watchdog:** WASM abort hook + memoryLimit + forceQuit (plan 87 fase 6) ([bcdf730](https://github.com/SkyRizzAI/kairo/commit/bcdf730d2f9ad31e6b7947335f7322612f4f961e))


### Bug Fixes

* **build:** resolve WASM build errors from plan 87 (plan 87 post) ([5fb3fa3](https://github.com/SkyRizzAI/kairo/commit/5fb3fa3d638803155e63556bd467965e7e9317e1))
* **ci:** add submodules:true to release workflow checkouts ([20cd677](https://github.com/SkyRizzAI/kairo/commit/20cd677af17e5cbea302f0bca9e6038edc0731b4))
* **wasm:** WASM OOB trap + ESP32 NVS crash from PSRAM-stack task ([ba46dfc](https://github.com/SkyRizzAI/kairo/commit/ba46dfcc20706dce1318648fbbb8a8c2bba0b883))

## [1.1.0](https://github.com/SkyRizzAI/kairo/compare/palanu-v1.0.0...palanu-v1.1.0) (2026-06-22)


### Features

* add compact virtual keyboard to FbconServer ([5ea0277](https://github.com/SkyRizzAI/kairo/commit/5ea0277ec587cd179d27a58da34284a92cb2d66d))
* animations & file browser ([6aedec8](https://github.com/SkyRizzAI/kairo/commit/6aedec84620104ad5060d58e70e8f734eb5d04ff))
* **app-model:** Plan 86 Fase 1 — argv forwarding + HostMode + terminal capture ([83cf6f0](https://github.com/SkyRizzAI/kairo/commit/83cf6f0dc0a41d6e9b85fe9d14faed8786167670))
* **app-model:** Plan 86 Fase 2 — raw canvas ABI (wasm_canvas.cpp) ([7f0f142](https://github.com/SkyRizzAI/kairo/commit/7f0f142ca75b66e4048f4eed00a8a2dc7fb12e8b))
* **app-model:** Plan 86 Fase 3 — retained UI ABI (wasm_ui.cpp) ([781894d](https://github.com/SkyRizzAI/kairo/commit/781894d960e34a95b3fc414732508a23c0b42396))
* **app-model:** Plan 86 Fase 4 — input + timing ABI (wasm_input.cpp) ([c2d3a0c](https://github.com/SkyRizzAI/kairo/commit/c2d3a0cb377283fdc86dc6853ddc8959e86e223a))
* **app-model:** Plan 86 Fase 5 — SDK DX (printf shim + display aliases) ([03a7e9f](https://github.com/SkyRizzAI/kairo/commit/03a7e9f03cefb9b9133326ab0f643d3992ffadbb))
* assets fonts ([0fa56d9](https://github.com/SkyRizzAI/kairo/commit/0fa56d96cc869666f1de994efb1d1d64339728bf))
* **assets:** Plan 82 — 3-tier asset architecture, system icons, .panim, system apps ([1c63c4f](https://github.com/SkyRizzAI/kairo/commit/1c63c4facf6798666a35b33ba24b2e9d6ef28e3b))
* **connectivity:** WiFi/BLE settings, remote auth, WebSocket transport (plans 72-75) ([8fa5a25](https://github.com/SkyRizzAI/kairo/commit/8fa5a25dfa977bb704f72b7edc29c7b1e510f0ba))
* **counter:** persist count via nema.storage.fs (plan 83 demo) ([c5ac6d2](https://github.com/SkyRizzAI/kairo/commit/c5ac6d26b82fd50ed8b88deed2bd29562e52ec19))
* display server, aether init, new app runtime concept ([1230296](https://github.com/SkyRizzAI/kairo/commit/1230296b3c21aec208dad9ff331174920b6aa286))
* **examples:** konsolidasi semua app examples ke ./examples/ ([1f5661c](https://github.com/SkyRizzAI/kairo/commit/1f5661c794aacc0191018856a47435d45fb449c4))
* **files:** file browser with context menu, text viewer, and file ops ([678d973](https://github.com/SkyRizzAI/kairo/commit/678d9739939546a9dc43e8b29c0030fabb549a6a))
* **fonts,dpm:** IoskeleyMono pack + render gate for display-off state ([beb89e5](https://github.com/SkyRizzAI/kairo/commit/beb89e57ae5971a18cfbab6d03a6116d74feac60))
* **hal:** secure element HAL scaffold (ISecureElement + SE050 + sim) — ADR 0005 ([0f283f1](https://github.com/SkyRizzAI/kairo/commit/0f283f17aa9977bb5dfe8070366432a364c7a74f))
* improve ui architecture ([e6ad544](https://github.com/SkyRizzAI/kairo/commit/e6ad544c253d5ec9317a4c4c3beb499f42c1c597))
* initialize first bad usb ky work ([143d2b3](https://github.com/SkyRizzAI/kairo/commit/143d2b397f220ec1a6117dfe2914682000cefd6c))
* interactive fbcon console + firmware OTA improvements ([0e74f00](https://github.com/SkyRizzAI/kairo/commit/0e74f00a10eef0f8e761424733876e5aaacef7f2))
* **keyboard:** redesign VirtualKeyboard + fix 2D navigation direction ([840fe5c](https://github.com/SkyRizzAI/kairo/commit/840fe5cc01e755104b9fde6846c5f5a210006192))
* **launcher:** custom app icons from .papp bundle (plan 84 fase 2) ([877b374](https://github.com/SkyRizzAI/kairo/commit/877b374c88991466b183a6e21607d19e0453639e))
* **launcher:** draw partial tile peeking off right edge as scroll hint ([1d94843](https://github.com/SkyRizzAI/kairo/commit/1d948431000f986b451dc555a6c8af014fee9911))
* **lcd:** implement sleep/wake to toggle backlight on hardware display off ([3a8f509](https://github.com/SkyRizzAI/kairo/commit/3a8f509261561e1050725ab231ceb13a8769b62d))
* **link,forge-cli:** extract shared protocol lib + standalone device CLI ([2e87ca5](https://github.com/SkyRizzAI/kairo/commit/2e87ca5e1815715119ba2c6793413e02758a35bc))
* **plan84/fase1:** JsApp::runProcess — headless JS CLI execution ([96cca52](https://github.com/SkyRizzAI/kairo/commit/96cca52eb61b3030afee9ea6502b8ff35db4ec7a))
* **plan84:** CLI dispatch + WASM guard + vector stability fix ([bd723e4](https://github.com/SkyRizzAI/kairo/commit/bd723e4194202000d0762f6d287654970cac3e64))
* QWERTY keyboard with full 2D navigation in FbconServer ([9168094](https://github.com/SkyRizzAI/kairo/commit/91680949934558032ba8e4b96fcd037dc1c72f80))
* **sdk:** Docker fallback for WASM builds (--rm, no leftover containers) ([9863750](https://github.com/SkyRizzAI/kairo/commit/9863750bcbc025d836ee53403c4f08759118028e))
* **sdk:** extend build.ts to compile WASM examples via wasi-sdk ([a773856](https://github.com/SkyRizzAI/kairo/commit/a77385625898dcb6a6b7583cbd9a6be8a6874f2f))
* **shell:** compact launcher skin — chevron-pennant carousel ([8a4a559](https://github.com/SkyRizzAI/kairo/commit/8a4a559584cf81f397e709c0b9745f74624b0238))
* **skyrizz-e32:** default 2x UI scale on first boot ([1db64d1](https://github.com/SkyRizzAI/kairo/commit/1db64d17025fca7584197129d4d49af73299012b))
* **skyrizz-e32:** package .panim assets into LittleFS flash image ([fc9bd6c](https://github.com/SkyRizzAI/kairo/commit/fc9bd6c512a30d872527275cf1a1b6da64c4bdb9))
* **storage:** plan 83 fase 1 — VFS restructure + NVS namespace fix ([2f8c711](https://github.com/SkyRizzAI/kairo/commit/2f8c711066a0f29cfd794ece35b74972836b0e17))
* **storage:** plan 83 fase 2 — AppStorage + StorageService + ctx.storage() ([601e797](https://github.com/SkyRizzAI/kairo/commit/601e797b2eb63eea70388b6e2dba2a967eb33b13))
* **storage:** plan 83 fase 3 — register StorageService in both platforms ([1212bcd](https://github.com/SkyRizzAI/kairo/commit/1212bcde67a8ae872d700876f88918e73430d2be))
* **storage:** plan 83 fase 4 — JS SDK file storage bindings ([94add04](https://github.com/SkyRizzAI/kairo/commit/94add049252a64246ce2ef9327eda1f11c75cad3))
* **storage:** plan 83 fase 5 — StorageSettingsScreen ([cd9b324](https://github.com/SkyRizzAI/kairo/commit/cd9b3247f01b2be81b4a4493cd89e40d42c1baf9))
* **ui:** desktop + themed launcher shell (plan 81) ([5e6abb5](https://github.com/SkyRizzAI/kairo/commit/5e6abb511f8aa69ebe0dd9af0e1cffebf2f89a91))
* **ui:** migrate all settings sub-screens to Flipper-style list UI ([5137dba](https://github.com/SkyRizzAI/kairo/commit/5137dba7c0a33bb3cc27b7f2664c13fb982184dd))
* **ui:** proportional fonts, ListView, scrollbar fix, live UI scale (plan 79) ([f432c9e](https://github.com/SkyRizzAI/kairo/commit/f432c9e3a83bbe61e1bb324e790c90c6b8177b28))
* **ui:** smart-scroll — focus-landmark stops + web-style top-align (plan 79) ([26d4f0b](https://github.com/SkyRizzAI/kairo/commit/26d4f0b0fe0fdc7d27bbaa5b8870f242854230e6))
* **ui:** WasmApp terminal fix + UI refactoring (plans 84-85) ([a6bd3bd](https://github.com/SkyRizzAI/kairo/commit/a6bd3bdc86261ea1ed04187e99e5f6d56d5f5c02))
* **wasm:** bare-metal SDK — wasm32-unknown-unknown, nema_api.h, ~1KB output (plan 85) ([f9a8b2a](https://github.com/SkyRizzAI/kairo/commit/f9a8b2acb930bfe5bead42cfaa43e92a0164d71d))
* **wasm:** headless WASM apps — install, launch, nema.* bridge (plan 84 fase 4a) ([1f4b2d0](https://github.com/SkyRizzAI/kairo/commit/1f4b2d019f8158ac773a8a82f2819560abdf4b51))
* **wasm:** Plan 86 Fase 6 — .papp.zip build output + Forge auto-unpack ([b7d1ee0](https://github.com/SkyRizzAI/kairo/commit/b7d1ee0d8220609d114957ab0ee95944b3dcf608))
* **wasm:** Plan 86 Fase 7 — WASM example apps (G1–G9 coverage) ([ae46d55](https://github.com/SkyRizzAI/kairo/commit/ae46d55056c1d711bec250253a2c1d3cc9528f56))
* **wasm:** terminal output screen — run output visible, press any key to exit ([e461757](https://github.com/SkyRizzAI/kairo/commit/e461757d52ae5c46b4242f3ac17463aa85b86054))


### Bug Fixes

* **about:** bump NodeArena from 96→320 to fix blank screen and hang ([64078a5](https://github.com/SkyRizzAI/kairo/commit/64078a5c7f472f02fd6a9800b602acb8ee8290d8))
* anchor fbcon prompt to bottom of screen ([28e1702](https://github.com/SkyRizzAI/kairo/commit/28e1702d696aeefd1ef1fae2688773b3732c2887))
* **apps:** update counter embedded bundle dengan storage support ([c23add5](https://github.com/SkyRizzAI/kairo/commit/c23add55e28830b84fc6128595cdc088d4f26434))
* **assets:** regenerate dolphin_sleep_panim.h with correct C array names ([79e2885](https://github.com/SkyRizzAI/kairo/commit/79e28851237591f01847ab2de86dddd676a145ce))
* **bluetooth:** remove status ListSection, show toggle state only ([8ab31ff](https://github.com/SkyRizzAI/kairo/commit/8ab31ff06e0cbf6e5b77452dbd64afb4a751864f))
* **build:** move papp_installer.cpp from core to aether ([6bff60b](https://github.com/SkyRizzAI/kairo/commit/6bff60bcde1de02e116bd432c88679c41a125e34))
* **build:** replace wasm3 add_subdirectory with static lib + IDF component wrapper ([a5f5b77](https://github.com/SkyRizzAI/kairo/commit/a5f5b77a5233b61dc0e94f61c1bb37079a6822ac))
* **canvas:** use floor-edge scaling to eliminate gap/overlap at fractional scale ([0815f6d](https://github.com/SkyRizzAI/kairo/commit/0815f6d5cf7c446439d0e7dbd4eaf027eb9abc36))
* **ci:** enable submodule checkout in all CI jobs ([fb89c24](https://github.com/SkyRizzAI/kairo/commit/fb89c241f8db250a764d9aa7bb835f2fc34a25a0))
* **core:** add missing &lt;string&gt; include to app_registry.h ([b7f41b6](https://github.com/SkyRizzAI/kairo/commit/b7f41b6baf75f88ecb890cdaece1e87428de1f2b))
* **desktop:** seed sleep.panim into WASM VFS so live wallpaper works in sim ([84e7b26](https://github.com/SkyRizzAI/kairo/commit/84e7b260a6fe277311ae2a0be9fa129cdbdf01d3))
* **dolphin:** split two-if line to silence -Werror=misleading-indentation ([81e6c77](https://github.com/SkyRizzAI/kairo/commit/81e6c7762ba051bd1cc65b6cb275833a52fa0361))
* **dpm:** locked state keeps display off — wake+LockScreen shown on first keypress ([6b2854d](https://github.com/SkyRizzAI/kairo/commit/6b2854d4afab10d2961ac21948a28d4df84dd360))
* **embedded_apps:** replace zero-size array with null pointer ([bfbf961](https://github.com/SkyRizzAI/kairo/commit/bfbf961ac85c62abead5cb67e9bd0caf4a1f1806))
* **esp32:** disable -Werror for wasm3_idf component (upstream warnings) ([3928da0](https://github.com/SkyRizzAI/kairo/commit/3928da01419de347ade662a41ad395a00e01f6ad))
* **esp32:** exclude tracer and uvwasi from wasm3_idf (compiler errors) ([68c6faa](https://github.com/SkyRizzAI/kairo/commit/68c6faae0cfcdd8decd34f0d50002715a2cdb4b0))
* **esp32:** remove legacy /anims dir + storage screen display fixes ([d798089](https://github.com/SkyRizzAI/kairo/commit/d79808957e3c34fb4c9a0c885325f942394ec9db))
* **esp32:** seed laptop.panim to LittleFS on boot — fixes blank live wallpaper ([6428c39](https://github.com/SkyRizzAI/kairo/commit/6428c39e95a669f5e59998a2baad2c9df18d8160))
* **esp32:** suppress -Wformat in wasm3_idf (uint32_t vs %d on Xtensa) ([5bf9f40](https://github.com/SkyRizzAI/kairo/commit/5bf9f40a22c6be323d9e156a8d9f01ec1ed2d702))
* **esp32:** suppress -Wmaybe-uninitialized on m3_parse.c via source file properties ([95f9158](https://github.com/SkyRizzAI/kairo/commit/95f915855cc3526cf5e81b56d2b740e40264711e))
* **esp32:** use wasm3_idf wrapper component instead of touching submodule ([0f825a2](https://github.com/SkyRizzAI/kairo/commit/0f825a293c4f30972e3d7203a1450b772e816fea))
* **examples:** add tsconfig.json with jsxImportSource=nema ([2ab9f77](https://github.com/SkyRizzAI/kairo/commit/2ab9f77d3ff1416065c06967f46244b36d642445))
* **files:** paste dirs, new folder, path truncation, upload cwd, busy overlay ([5f02d61](https://github.com/SkyRizzAI/kairo/commit/5f02d61d9ca07f530b0ec464825c3a0833e66f3e))
* **forge:** proxy GitHub release downloads to bypass COEP restriction ([8e8a324](https://github.com/SkyRizzAI/kairo/commit/8e8a324a473e019487328c147d7b2cab38303240))
* **idl:** wire storage.fs ke JS + method names jadi camelCase ([7aeecd8](https://github.com/SkyRizzAI/kairo/commit/7aeecd8213ae06736019c414d8c6adb6d142ee33))
* **input:** swap default key mapping — Up/Down=navigate, Left/Right=adjust ([7c99e55](https://github.com/SkyRizzAI/kairo/commit/7c99e555f08cdd696a16b1283fdf08fd3028912e))
* **installer:** JS bundle entry lookup case-sensitive mismatch ([6c61ebb](https://github.com/SkyRizzAI/kairo/commit/6c61ebbe4f38b5076083000b2ead8a55ae73613b))
* **js:** route NemaHostImpl storage through pre-warmed AppContext ([50f14ab](https://github.com/SkyRizzAI/kairo/commit/50f14ab44eaf6215a9f1b49c3a7af12b68831c6a))
* keyboard open by default with cursor on Enter key ([30e03bd](https://github.com/SkyRizzAI/kairo/commit/30e03bd95e3ef179e0910ce3533626dc08adac54))
* marque text settings list ([1fc280f](https://github.com/SkyRizzAI/kairo/commit/1fc280f9f60b1df3827f69b18409d1b63cb6005e))
* mkdirAll() buat semua component path sebelum write/move. ([2f7491f](https://github.com/SkyRizzAI/kairo/commit/2f7491f3fea929fcc94063da8165e7abdc418bb8))
* **papp:** use manifest entry field for WASM filename lookup ([43c6884](https://github.com/SkyRizzAI/kairo/commit/43c6884210cdecbf61d246970d0b010349460376))
* **remote:** replace status ListSection with plain info row to prevent bold/clip ([c89dacf](https://github.com/SkyRizzAI/kairo/commit/c89dacf09d52ab3f1535f08a18d6fd693173c0ea))
* restore wasm3 submodule mapping in .gitmodules ([9ca5f4a](https://github.com/SkyRizzAI/kairo/commit/9ca5f4a5150a6f7b8c0a104b74f634f40199ba98))
* run script scree in badusb ([d187985](https://github.com/SkyRizzAI/kairo/commit/d18798526efa1cbab61e4a9675fc62ec974770f4))
* **status_bar:** shrink battery icon from 25x8 to 16x8 for proportional display ([c5f9444](https://github.com/SkyRizzAI/kairo/commit/c5f9444b40340cb7f7757d09348de6a03f362855))
* **status_bar:** wifi icon only shows when wifi is actually connected ([8042c8f](https://github.com/SkyRizzAI/kairo/commit/8042c8f72dc2b4148a7777618beb898ad295ef20))
* **storage-screen:** dangling pointer — store value strings in vals_ member ([2999fd6](https://github.com/SkyRizzAI/kairo/commit/2999fd69d1c4f28f24925f48e964a0be5fec08e3))
* **storage-screen:** volume Unknown + separator glyph issue ([54ea5ef](https://github.com/SkyRizzAI/kairo/commit/54ea5ef798aee865418b7d1cf05e99338feb8ac0))
* **storage:** cek exist dir sebelum mkdirAll di write() ([f999500](https://github.com/SkyRizzAI/kairo/commit/f9995007d589db76b8d30d8654e66dc6d0fa7d89))
* **storage:** pre-resolve AppStorage base path on GUI thread ([a9c60e2](https://github.com/SkyRizzAI/kairo/commit/a9c60e2710fe8827519031ea141f464a8213edc6))
* **storage:** recursive mkdir — SD write gagal karena /sd/data/ belum ada ([2f7491f](https://github.com/SkyRizzAI/kairo/commit/2f7491f3fea929fcc94063da8165e7abdc418bb8))
* **storage:** uint32_t format specifier cast for ESP32 -Werror ([d16d0cb](https://github.com/SkyRizzAI/kairo/commit/d16d0cb92aab57ec2e5df8c7bb6130e114b678eb))
* **tests:** add missing &lt;string&gt; include in wifi_contract_test ([1c3565e](https://github.com/SkyRizzAI/kairo/commit/1c3565e91efa81e786a192c6aa52a1263a980adc))
* **tooling:** guard rails supaya bug storage.fs tidak terulang ([37bb9d1](https://github.com/SkyRizzAI/kairo/commit/37bb9d16f9f7f9c0e03ad42b578f7360e5563d5a))
* **ui:** BadUSB as ComponentScreen + focus reset on all settings screens ([f8a7887](https://github.com/SkyRizzAI/kairo/commit/f8a7887313d6a0a1a678c43125d4b5eb2c56d2b1))
* **ui:** BadUsbApp — show HID status and script count in info panel ([f9556f2](https://github.com/SkyRizzAI/kairo/commit/f9556f2cc0e7752bdb8a1006171e00c7bd79ef3d))
* **ui:** ellipsis gap, value centering in ListInputRow ([da9ba53](https://github.com/SkyRizzAI/kairo/commit/da9ba53466b11a0fb5f64a9d8224a1927b2af2f0))
* **ui:** marquee clip overflow + continuous redraw for animation ([f9ef04c](https://github.com/SkyRizzAI/kairo/commit/f9ef04c8c80c7d2a482f4d3c327316b1c1099209))
* **ui:** marquee copy-2 position relative to virtual text left edge ([8ff9616](https://github.com/SkyRizzAI/kairo/commit/8ff961603bd2ff7a3fe7ef4993ab0d05356f156e))
* **ui:** PS launcher — selected tile flush-left on tight screens ([c396788](https://github.com/SkyRizzAI/kairo/commit/c396788655cf2a4f24549e704323f31720b6a14d))
* **ui:** reach trailing info + keep section headers on scroll (plan 79) ([586cf6a](https://github.com/SkyRizzAI/kairo/commit/586cf6a3f8c25fae895f6e3ef5d120221ee41706))
* **ui:** remove page TitleBar from all settings sub-screens ([f2c49bf](https://github.com/SkyRizzAI/kairo/commit/f2c49bfc3c9ccf64e54f7b3c9704abde55159d19))
* **ui:** rename Display to Display & Appearances in settings ([61a0b30](https://github.com/SkyRizzAI/kairo/commit/61a0b303a971640277364783876c08bd5df3848e))
* **ui:** rewrite marquee for proportional fonts — eliminates stutter ([e40e437](https://github.com/SkyRizzAI/kairo/commit/e40e43795d657db2ebb98dbcfba66d3be24c4aec))
* **ui:** scale scroll context proportional to viewport height ([545fa7f](https://github.com/SkyRizzAI/kairo/commit/545fa7f39fda2105e215da6c9ca443c87d36a15b))
* **ui:** scroll buffer, infinite marquee, and wrap-prevention scroll ([c09fd26](https://github.com/SkyRizzAI/kairo/commit/c09fd26ecaf04152820fe33d6a708286407318f5))
* **ui:** slow marquee to 40px/sec, rate-limit redraws to 15fps ([6ec6680](https://github.com/SkyRizzAI/kairo/commit/6ec668075cebd09af92134ca63ae5fa986a45d15))
* **ui:** status bar icons invisible on inverted bar — wrong draw polarity ([ec75d49](https://github.com/SkyRizzAI/kairo/commit/ec75d498cb855b2e9687975aff3c1d6c319d8666))
* **ui:** status bar OFF reclaims full-height content area ([29908ed](https://github.com/SkyRizzAI/kairo/commit/29908ed731b74e6c56699c79405e7965a8315720))
* **workspace:** examples depend on @palanu/app-sdk, not nema ([7a0188d](https://github.com/SkyRizzAI/kairo/commit/7a0188def696ed29a242c89f7414435dde187072))


### Performance Improvements

* **flash:** shrink spiffs partition 5.8MB→512KB to cut flash time ~13s→~1s ([c6b53ce](https://github.com/SkyRizzAI/kairo/commit/c6b53ce8fe74df0604d24707b9c4afb8d05b6124))


### Reverts

* **sdk:** remove Docker fallback from WASM build ([c106ac0](https://github.com/SkyRizzAI/kairo/commit/c106ac0672cc7c39d040f7072684f41168edf28e))

## 1.0.0 (2026-06-14)


### Features

* Add I2S diagnostics and instrument audio bring-up ([8461fbf](https://github.com/SkyRizzAI/kairo/commit/8461fbf85fd635366b28c5bd8a22206d77924ecf))
* Add ProfileSettingsScreen ([0b2d6f3](https://github.com/SkyRizzAI/kairo/commit/0b2d6f3c40627b54d12eaed4c001c3e9f506114e))
* ble, usb & forge ([47c237b](https://github.com/SkyRizzAI/kairo/commit/47c237b130e46424178dce0b450aeeffe9714f65))
* **core:** add `version`/`ver` CLI command ([14542ed](https://github.com/SkyRizzAI/kairo/commit/14542eda9b351f8af79b6053a9cc0fa202dee6ca))
* **core:** finish Plan 42/43 deferred items — liveness bridge, fallback, boot policy ([f232a26](https://github.com/SkyRizzAI/kairo/commit/f232a26c14f2b139445ff758db9d89a5b2af2e09))
* **core:** Plan 42 Fase 1 — two-axis capability registry + catalog ([2c15089](https://github.com/SkyRizzAI/kairo/commit/2c150895f6d2ffd6db6473b51fc8d2ab4b86617a))
* **core:** Plan 42 Fase 4 — resource liveness wiring (mechanism + safe owners) ([95b52cd](https://github.com/SkyRizzAI/kairo/commit/95b52cdbdc3acd4200ce4ec36ed3e700a19a4604))
* **core:** Plan 43 Fase 1 — extract IDisplayServer + PixelateServer ([6167a90](https://github.com/SkyRizzAI/kairo/commit/6167a90098730b4b348a065c1352ac176baf75f5))
* **core:** Plan 43 Fase 2/3 — runtime backend swap + FbconServer + CLI `display` ([903fb8f](https://github.com/SkyRizzAI/kairo/commit/903fb8f71a2cb297fd3f876be235b20f98b3e119))
* **core:** Plan 44 Fase 1 — CLI shell with per-connection sessions ([f94d976](https://github.com/SkyRizzAI/kairo/commit/f94d976a3d90ca5ab76b6ba0d43fe78ecbbcebe0))
* **core:** Plan 46 — `ps` process monitor (services + apps + sessions) ([3febaa2](https://github.com/SkyRizzAI/kairo/commit/3febaa2ed753e0cc5a139eca4f18a9bae4c537a0))
* firmware OTA reliability, auto-restart, binary rebrand, and GitHub Releases UI ([661dbed](https://github.com/SkyRizzAI/kairo/commit/661dbeda220c022c197eef47375e204be1c16a77))
* **firmware:** Plan 39 — firmware OTA (device side), transport-agnostic, no secure boot ([8f15441](https://github.com/SkyRizzAI/kairo/commit/8f1544154bf31ef1b66040cf47e340bda68f20c1))
* **forge:** Plan 39 — "Update firmware" panel + OTA streaming (remote section) ([cdf586f](https://github.com/SkyRizzAI/kairo/commit/cdf586f3be99255dff502c5523919cd290f511c5))
* **forge:** show the CLI terminal by default in the simulator ([2159841](https://github.com/SkyRizzAI/kairo/commit/2159841b0041b429e325a1961eaab2ddec979091))
* improve frame buffer display, increase max fps ([6a9a167](https://github.com/SkyRizzAI/kairo/commit/6a9a1671d41f29cde7dac0d11cb68a389ef424f5))
* initialize firmware + simulator + example board ([f301da8](https://github.com/SkyRizzAI/kairo/commit/f301da8957fdf2fc60980f27aa24fd123421b5c4))
* initialize vfs & cli ([9fbdf75](https://github.com/SkyRizzAI/kairo/commit/9fbdf750c17764f967660f8765a6435820f8582f))
* initialze native component ui & board profile ([dd43e8d](https://github.com/SkyRizzAI/kairo/commit/dd43e8dc60a2bf1e54f63481067a09a4a15704c4))
* Plan 44 Fase 4 — shell cwd prompt in Forge; finish Plan 44 ([0e65c3a](https://github.com/SkyRizzAI/kairo/commit/0e65c3ab29ef8794dd93eea066481f1be8b54c92))
* Plan 45 — multi-session CLI (independent shells via session-id) ([76770e8](https://github.com/SkyRizzAI/kairo/commit/76770e878fb9910c28bd12eb3643417b5da710e5))
* remote layout and dynamic layout forge remote device visualizer ([9e3eace](https://github.com/SkyRizzAI/kairo/commit/9e3eacecf7f7edd40972aadb622e3a6a6fdc67e7))
* testable OTA flow in the WASM simulator (dry-run) ([2b869a4](https://github.com/SkyRizzAI/kairo/commit/2b869a4ba18f6fbfa1d82450a7e0591e3c851008))
* **tools:** dev OTA build — local version increment, gitignored ([541a493](https://github.com/SkyRizzAI/kairo/commit/541a49328c8bcf8fbdb125a96c53dd5896d9f682))
* touch, input gesture abstraction, lcd invertion and init camera, ([f97b70f](https://github.com/SkyRizzAI/kairo/commit/f97b70f078cd067fd47f53b3befcdfc7ac93b553))
* Use Kairo-owned HWCDC for ESP32 USB CDC ([0b10cbc](https://github.com/SkyRizzAI/kairo/commit/0b10cbc972152a2f162693f540a233c218ed034b))
* Use legacy I2S driver and drop dev-board docs ([b1811c8](https://github.com/SkyRizzAI/kairo/commit/b1811c8f7f5b6a0fce3e2c35a1d1eec178385317))
* user system + fix app js stack ([45ec690](https://github.com/SkyRizzAI/kairo/commit/45ec6909f11395ef20e332b71dda252ce445d82d))


### Bug Fixes

* apps js and semver ([4430584](https://github.com/SkyRizzAI/kairo/commit/443058433f9a03c36cb6637ebe02f5c2f7142dab))
* **core:** default boot to fbcon console (CLI-first), not pixelate UI ([8c69bb9](https://github.com/SkyRizzAI/kairo/commit/8c69bb9b8de40a203a99338fb21466ac133ca6fc))
* **forge:** use dynamic public env for FIRMWARE_REPO to avoid 500 on missing .env ([10fdb3e](https://github.com/SkyRizzAI/kairo/commit/10fdb3eeac06fde712af4e096e3688b4c65e5de6))
* **ota:** protocol-version handshake — diagnose stale firmware instead of dying at 0% ([e7858c4](https://github.com/SkyRizzAI/kairo/commit/e7858c41b7b78c830da026c27f3cefe6384a9076))
* **ota:** real-time progress + status log; long Begin timeout; sim never halts ([2eb03d3](https://github.com/SkyRizzAI/kairo/commit/2eb03d32a2c3e8a73aa8b48a68288a130defefc5))
* **ota:** reliability — atomic link send + 1KB chunks ("lost connection" mid-upload) ([6bdd48d](https://github.com/SkyRizzAI/kairo/commit/6bdd48dcd5ac750ae01a4ce533e7bcd17744a210))
* **ota:** resilient upload — offset-based chunks + idempotent retry ([7285a96](https://github.com/SkyRizzAI/kairo/commit/7285a961241f9afa7c6d4707a79f5a9db01cd283))
* remote unused dir ([7e71458](https://github.com/SkyRizzAI/kairo/commit/7e714582742c736864baa65fbb7dc2440371e693))
* **skyrizz-e32:** remove I2S diagnostic that broke the ESP32 build ([1bd079a](https://github.com/SkyRizzAI/kairo/commit/1bd079ae431713ffb128176b73a625fed7de50c7))
* **wasm:** rename build target and artifacts nema → palanu ([d197b15](https://github.com/SkyRizzAI/kairo/commit/d197b151cf48d5d22ce7f40eb30c653db6a8d17a))
