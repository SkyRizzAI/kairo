# Changelog

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
