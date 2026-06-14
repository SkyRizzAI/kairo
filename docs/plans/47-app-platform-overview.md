# 47 — App Platform Overview & Architecture Map

> Index seri **Palanu App Platform** (Plan 47–59). Memetakan layering, urutan
> dependensi, dan keputusan arsitektur lintas-plan. Bukan fitur — ini peta.

- Status: 🟧 Detail draft (belum diimplementasi)
- Milestone: M13 (App Platform)
- Depends on: Plan 42 (capability), 43 (display server), 44–46 (CLI shell, multi-session, process monitor)

---

## Goals

- Memberi peta tunggal: bagaimana 13 plan ini saling bertumpuk (pondasi → runtime).
- Mengunci keputusan lintas-plan yang dipakai semua dokumen berikutnya.
- Glossary: System API, IDL, display server, UI SDK, compositor, surface, process, runtime tier.

## Keputusan (lintas-plan, dikunci)

- **Model mental:** App = *proses Unix* (argv/stdin/stdout/stderr/exit) yang
  opsional jadi *klien display-server* (minta surface lalu menggambar).
- **Tiga runtime, satu host API:** C built-in (compiled), WASM (wasm3), JS
  (QuickJS-ng) — semuanya adapter tipis di atas **Host API C++ runtime-agnostic**.
- **UI = per-display-server, BUKAN universal.** Tiap server punya UI SDK sendiri
  (font/widget/theme). App = `headless` (jalan di mana pun) atau `ui:<server>`
  (terikat satu server). **Hanya System API non-UI yang bersama.** Sekarang fokus Aether.
- **Capability = izin meng-import sebuah interface System API.**
- Urutan kerja: System API/IDL → UI SDK model → Display Server arch → Aether →
  Theming → Process model → Surface model → Runtimes → Packaging.

## Peta seri

| Plan | Fitur | Lapisan |
|---|---|---|
| 48 | Nema System API & IDL (non-UI, SSOT) | Pondasi |
| 49 | SDK Binding Generator & Docs/Parity | Pondasi |
| 50 | UI SDK Model (per-server) + Import/Binding | Pondasi UI |
| 51 | Display Server Architecture & Negotiation | Pondasi UI |
| 52 | Aether UI SDK (pixelete/Flipper, flexbox/node) | Implementasi UI |
| 53 | Aether Theming & Asset Packs | Implementasi UI |
| 54 | Process Model & Shell Execution | Pondasi App |
| 55 | Surface & Window Model | Pondasi App |
| 56 | App Runtime Architecture + Native (C) | Runtime |
| 57 | App Runtime: WASM | Runtime |
| 58 | App Runtime: JS (QuickJS-ng) | Runtime |
| 59 | App System: Manifest, Packaging (.papp) & Launcher | Produk |

---

## Diagram layering

Tiga sumbu yang sering dikacaukan jadi satu — peta ini memisahkannya:
**(A) System API non-UI bersama**, **(B) UI per-display-server**, **(C) tiga runtime
tier**. Sebuah app memilih satu titik di tiap sumbu lewat manifest (Plan 59).

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  APP  (manifest: id, runtime, display_server, mode, needs[])   — Plan 59       │
│                                                                                 │
│   runtime ∈ { c, wasm, js }        display_server ∈ { headless, aether }        │
└───────────────┬───────────────────────────────────┬─────────────────────────────┘
                │ (C) adapter runtime               │ (B) import UI per-server
                ▼                                    ▼
   ┌─────────────────────────────┐      ┌────────────────────────────────────────┐
   │  RUNTIME TIER (adapter tipis)│      │  UI SDK per-server (TIDAK universal)    │
   │  56 C built-in  (compiled)   │      │  50 model import/binding                │
   │  57 WASM        (wasm3)      │      │  51 negosiasi launch (server tersedia?) │
   │  58 JS          (QuickJS-ng) │      │  52 Aether UI SDK (pixelete + flexbox)  │
   │     ── adapter ke ──         │      │  53 Aether theming + asset packs        │
   └──────────────┬──────────────┘      └───────────────────┬────────────────────┘
                  │                                          │ minta surface
                  ▼                                          ▼
   ┌──────────────────────────────┐      ┌─────────────────────────────────────────┐
   │ (A) HOST API C++ runtime-agno │      │  55 Surface & Window Model (compositor) │
   │ ── System API NON-UI bersama ─│      │  43/51 IDisplayServer (aether|fbcon|…)  │
   │ 48 IDL (SSOT) → 49 generator  │      └─────────────────────────────────────────┘
   │ log device storage profile    │
   │ http fs wifi ble events tasks  │      ┌─────────────────────────────────────────┐
   │ timer power audio camera …     │      │  54 Process Model (argv/stdin/stdout/   │
   │   ↑ capability-gated (Plan 42) │      │      stderr/exit) di atas CLI shell      │
   └──────────────┬─────────────────┘      │      44 shell · 45 multi-sess · 46 ps    │
                  │                         └─────────────────────────────────────────┘
                  ▼
   ┌──────────────────────────────────────────────────────────────────────────────┐
   │  KERNEL / SERVICES Nema  (ServiceContainer, drivers, capability registry)      │
   │  — TIDAK semua di-expose: IDL = subset terkurasi & aman (Plan 48)              │
   └──────────────────────────────────────────────────────────────────────────────┘
```

Tiga prinsip yang dibaca dari diagram:

1. **Hanya kotak (A) yang bersama** semua app & semua server. UI (B) terpisah
   **per-server** — app `ui:aether` tak portabel ke server lain (keputusan dikunci).
2. **Runtime (C) ortogonal** terhadap UI: app C bisa `aether`, app JS bisa
   `headless`. Tier hanya soal *bahasa eksekusi*, bukan *punya layar atau tidak*.
3. **Process (54) selalu ada**; surface (55) opsional. App = proses dulu, klien
   display-server kedua. Headless = berhenti di proses; UI = lanjut minta surface.

---

## Graf dependensi (13 plan)

Panah `A → B` = "B butuh A". Pondasi di kiri, produk di kanan.

```
            Plan 42 (capability) ─┐
            Plan 43 (display srv) ─┼──────────────────────────────────────┐
            Plan 44/45/46 (shell) ─┘                                       │
                                                                           │
  ┌── 48 System API/IDL ──┬── 49 generator ──┬── 56 runtime arch (C) ──┬── 57 WASM
  │   (non-UI SSOT)        │                  │                          └── 58 JS
  │                        │                  │
  │                        └── 50 UI SDK model ┬── 51 display-srv negosiasi
  │                                            │        │
  │                                            │        └── 55 surface/window ──┐
  │                                            └── 52 Aether UI SDK ── 53 theming│
  │                                                     │                        │
  54 process model (← 44/45/46) ───────────────────────┴── 55 surface ──────────┤
                                                                                 │
                          56 + 57 + 58 (runtimes) ─────────────────────────────┐│
                                                                               ▼▼
                                                          59 manifest/.papp/launcher
                                                          (+ Plan 37 custom apps,
                                                           + Plan 38 LittleFS persist)
```

Dependensi eksplisit per plan (dari stub masing-masing):

| Plan | Depends on | Blocks |
|---|---|---|
| 48 System API/IDL | 42 | 49, 50, 56/57/58 |
| 49 generator | 48 | 56/57/58, docs |
| 50 UI SDK model | 48, 49 | 51, 52, 59 |
| 51 display-srv negosiasi | 43, 50 | 52, 55 |
| 52 Aether UI SDK | 50, 51, 53 | 55 |
| 53 Aether theming | 50, 52 | — |
| 54 process model | 44, 45, 46 | 55, 56/57/58 |
| 55 surface/window | 51, 52, 54 | runtimes (sisi UI) |
| 56 runtime arch + C | 48, 54, 55 | 57, 58 |
| 57 WASM | 49, 54, 55, 56 | 59 |
| 58 JS | 49, 54, 55, 56 | 59 |
| 59 manifest/.papp | 55, 56, 57, 58 (+37, 38) | — (produk) |

**Tiga rantai kritis** yang menentukan urutan:

- **Rantai non-UI**: 48 → 49 → 56 → {57, 58}. (Apa yang app boleh panggil.)
- **Rantai UI**: 50 → 51 → 52 → 53, lalu 55 (jahit app↔compositor). (Bagaimana
  app menggambar.)
- **Rantai proses**: 44/45/46 → 54 → 55. (Bagaimana app dijalankan & dimatikan.)

Ketiganya bertemu di **56** (runtime arch menyatukan System API + process + surface)
lalu **59** (packaging membungkus jadi produk).

---

## Urutan kerja yang disarankan

Mengikuti keputusan dikunci ("System API/IDL → … → Packaging"), dengan alasan:

1. **48 System API/IDL** — SSOT permukaan non-UI. Semua hilir (generator, runtime,
   docs) di-generate dari sini → kerjakan **dulu** supaya parity by construction.
2. **49 generator** — begitu IDL ada, generator membuat binding host + SDK + docs.
   Tak ada binding tangan; ini mengunci pipeline build.
3. **50 UI SDK model** — pola per-server (namespace `aether:ui`, import per-runtime).
   Bisa paralel dgn 49 (keduanya cuma butuh 48).
4. **51 display-server negosiasi** — perluas `IDisplayServer` (Plan 43): server
   deklarasi UI SDK + capability; launch dicocokkan. Gerbang UI app.
5. **52 Aether UI SDK** — implementasi UI pertama (pixelete + flexbox + compositor).
   Butuh 50/51 + 53 (theming dipakai widget).
6. **53 Aether theming** — theme/asset-pack milik Aether; bisa interleave dgn 52.
7. **54 process model** — argv/stdio/exit di atas shell (44–46). **Paralel** dgn
   jalur UI (cuma butuh shell) — bisa dimulai lebih awal.
8. **55 surface/window** — jahit app↔compositor (butuh 51, 52, 54). Titik temu
   jalur UI & jalur proses.
9. **56 runtime arch + C** — Host API runtime-agnostic + tier C built-in. Butuh
   48 + 54 + 55 → kerjakan **setelah** ketiga rantai ketemu.
10. **57 WASM** & **58 JS** — dua tier tambahan di atas 56; bisa **paralel**
    (keduanya konsumsi SDK generator + adapter 56).
11. **59 manifest/.papp/launcher** — bungkus semua jadi produk; melanjutkan Plan 37
    (custom apps) & 38 (persistensi LittleFS). Terakhir karena butuh semua runtime.

> Jalur paralel yang aman: **(rantai non-UI 48→49→56)** dan **(rantai UI
> 50→51→52→53)** berbagi hanya 48; bisa dua orang/agent. **54 (proses)** bisa jalan
> kapan saja setelah shell (44–46). Semua bertemu di **55→56**, lalu **57/58**
> paralel, lalu **59**.

---

## Glossary

| Istilah | Arti di Palanu |
|---|---|
| **System API** | Permukaan non-UI yang di-expose kernel/services ke app — `log, device, storage, profile, http, input, fs, wifi, ble, events, tasks/timer, power, audio, camera`. **Bersama** semua runtime & semua display-server. SSOT = IDL (Plan 48). Bukan cermin `rt.*`: subset terkurasi & aman. |
| **IDL** (Interface Definition Language) | Sumber tunggal (WIT-style atau JSON/TOML sendiri) yang mendefinisikan System API. Dari sini di-generate host registration + SDK (C/WASM/JS) + docs + matriks parity ("OpenAPI-nya Palanu"). Plan 48/49. |
| **Display server** | Backend renderer yang bisa diganti runtime (`IDisplayServer`, Plan 43): `aether` (canvas/UI), `fbcon` (console/TTY, fallback), `lvgl` (board warna). App `ui:<server>` terikat ke satu server; di-launch dari CLI di atas TTY. |
| **UI SDK** | Pustaka UI **milik satu display-server** (namespace, font, widget, theme) — `aether:ui` sekarang. **Tidak universal**: server lain punya UI SDK sendiri. Diakses app via import per-runtime (Plan 50). |
| **Compositor** | Bagian display-server yang memiliki N surface/viewport (mis. status-bar plus app) dan menyusunnya jadi satu frame. Aether: pixelete 1-bit; kebijakan awal single-foreground (Plan 52/55). |
| **Surface** | "Window" yang dimiliki app: app **selalu** menggambar ke surface-nya, **tak pernah** mengasumsikan memiliki layar. Jumlah-window = kebijakan compositor, bukan app → multi-window "nyaris gratis" nanti (Plan 55). |
| **Process** | App sebagai proses Unix: `argv[]`, `stdin/stdout/stderr`, `cwd/env`, `exit(code)`; mendukung pipe (`appA \| appB`). Kernel sediakan ~5 syscall; parser arg (commander/clap) = userspace (Plan 54). |
| **Runtime tier** | Salah satu dari tiga mesin eksekusi: **C built-in** (di-compile ke firmware, tercepat/tepercaya), **WASM** (wasm3, portabel+sandboxed), **JS** (QuickJS-ng, DX modern). Semua = adapter tipis ke Host API yang sama (Plan 56/57/58). |
| **Host API** | Lapisan C++ runtime-agnostic yang mengimplementasi System API + surface + process. Dirancang **sebelum** runtime mana pun; tiap tier hanya membungkusnya (Plan 56). |
| **Capability** | Izin meng-import sebuah interface System API (Plan 42). Interface core (`log/device/events/tasks`) selalu ada; sisanya gated. Manifest `needs[]` = daftar capability yang diminta app; dicek saat launch. |
| **Headless / `ui:<server>`** | Dua kelas app. **Headless** = System API saja, jalan di board mana pun (CLI). **`ui:aether`** = terikat ke satu display-server, butuh server itu tersedia untuk bagian UI-nya. **Hybrid** = headless yang bisa angkat UI bila server ada. |
| **.papp / PAPP1** | Format paket app (pengganti `.kapp`/KAPP1). Dua amplop: single-file script & bundle (folder + assets/icon). Transfer/disk form = **TOC-concatenated** (bukan zip) untuk streaming via KLP + LittleFS. Runtime di belakang sama (Plan 59). |
| **Manifest** | Deklarasi app: `id, name, version, runtime, display_server, mode, needs[], icon, category, api_version`. Entry-0 di `.papp` → launcher baca metadata tanpa load code (Plan 59). |
