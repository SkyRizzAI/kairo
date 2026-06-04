# 26 — Simulator UI Overhaul

> Simulator menjadi profesional: resolusi display bisa dipilih dan diganti live,
> layout UI diperbaiki dengan panel terpisah (WiFi | Display | System), tombol
> hardware ditampilkan seperti perangkat fisik asli, dan SimDisplay mendukung
> resolusi dinamis sehingga Plan 25 (Adaptive UI) bisa ditest di berbagai ukuran.

- Status: ✅ Phase 1 (dynamic resolution) + Phase 2 (3-column layout, settings tabs, hardware buttons) + Phase 3 (bezel polish) DONE. Verified live: 264×176 → 400×300 → 128×64 resolution switching with adaptive UI. Note: home-screen wordmark needs a compact variant for tiny (<160px) panels — content concern, not layout.
- Milestone: M8 (Hardware Portability)
- Depends on: 25 (Adaptive UI — Phase 1 idealnya selesai dulu)

---

## Kondisi sekarang (audit)

| Aspek | Status |
|-------|--------|
| `SimDisplay` W, H | `static constexpr W=264, H=176` di header — hardcoded |
| Buffer display | `uint8_t buf_[W * H]` — stack array 46 KB, tidak bisa resize |
| Binary spawn | Hanya `KAIRO_SIM_JSON=1` env var, tidak ada dimensi |
| Frontend layout | Grid `570px \| 1fr`. Left: display + kontrol campur aduk |
| Controls | WiFi networks, buttons, runtime controls semua di satu panel panjang |
| Tombol hardware | Grid CSS biasa, tidak visual seperti D-pad fisik |
| Resolusi | Hardcoded `W=264, H=176, SCALE=2` di `DisplayPanel.tsx` |

---

## Daftar resolusi IoT yang didukung

Preset yang akan ditawarkan di panel Display:

### E-ink / E-paper (prioritas utama untuk Kairo)

| Label | Resolusi | Perangkat |
|-------|----------|-----------|
| 1.54" E-ink | 200×200 | Waveshare 1.54" |
| 2.13" E-ink | 250×122 | Waveshare 2.13" |
| 2.7" E-ink ★ | **264×176** | GDEY027T91 — dev board kita (default) |
| 2.9" E-ink | 296×128 | Waveshare 2.9" |
| 4.2" E-ink | 400×300 | Waveshare 4.2" |
| 5.83" E-ink | 640×384 | Waveshare 5.83" |
| 7.5" E-ink | 800×480 | Waveshare 7.5" |

### OLED (monochrome)

| Label | Resolusi | Perangkat |
|-------|----------|-----------|
| 0.91" OLED | 128×32 | SSD1306 small |
| 0.96" OLED | 128×64 | SSD1306 — paling populer di embedded |
| 1.3" OLED | 128×64 | SH1106 |

### Custom
User input `width` × `height` bebas (min 64×32, max 1024×768).

---

## Desain UI baru

### Layout tiga panel

```
┌──────────────────────────────────────────────────────────────────┐
│  Kairo Simulator  ● running   simulator/simulator · v.dev         │
├─────────────────────┬──────────────────────┬─────────────────────┤
│                     │                      │                     │
│   DEVICE            │   SETTINGS           │   LOGS / EVENTS     │
│   ─────             │   ─────────          │   ────────────      │
│  ┌─────────────┐    │  [WiFi][Display][Sys]│  ● Logs             │
│  │             │    │  ─────────────────── │  15:01:00 INFO ...  │
│  │   DISPLAY   │    │  WiFi tab:           │  15:01:01 DEBUG ... │
│  │   (canvas)  │    │  ┌─────────────────┐ │                     │
│  │             │    │  │ MyHomeWiFi      │ │  ● Events           │
│  └─────────────┘    │  │ ████████ -42dBm │ │  ClockTick ...      │
│                     │  │ ○ CoffeeShop   │ │  NetworkConn ...    │
│  ┌───┐              │  └─────────────────┘ │                     │
│  │ ▲ │              │  Connected: MyHome   │  ● Services         │
│  │◄ ►│  [● SEL]     │  IP: 192.168.1.5    │  SimDisplay Running │
│  │ ▼ │  [✕ CAN]     │                      │  SimWifi   Running  │
│  └───┘              │                      │                     │
│                     │                      │                     │
│  [Boot][⏹][↺]      │                      │                     │
└─────────────────────┴──────────────────────┴─────────────────────┘
```

**3 kolom:**
- **Kiri (Device)**: display + hardware buttons + runtime controls
- **Tengah (Settings)**: tabbed panel — WiFi / Display / System
- **Kanan (Logs)**: logs, events, services — read-only info

### Tombol hardware (Device panel)

Tampil seperti perangkat fisik:

```
         [ ▲ ]
   [ ◄ ] [   ] [ ► ]        [ ● SELECT ]
         [ ▼ ]              [ ✕ CANCEL ]
```

D-pad (kiri): 4 tombol panah dalam pola cross 3×3 (cell tengah kosong).
Action (kanan): SELECT (bulat/hijau) dan CANCEL (X/merah) disusun vertikal.

CSS: CSS Grid 3×3 untuk D-pad, flexbox vertikal untuk action buttons.

### Panel Settings — tiga tab

**Tab: WiFi**
- List jaringan (SSID, password, RSSI slider, toggle online/offline)
- Status koneksi aktif di bawah
- Tombol "+ Add Network"

**Tab: Display**
- Resolution picker (dropdown preset + custom input W × H)
- Theme selector (E-Ink / Phosphor / Amber)
- Scale indicator (menampilkan scale yang akan dipakai di Canvas setelah Plan 25 Phase 2)
- Tombol "Apply" → trigger restart dengan resolusi baru

**Tab: System**
- Boot / Shutdown / Restart buttons (dipindahkan dari Device panel ke sini)
- Inject event (dipindahkan ke sini juga)
- Info: platform, board, firmware version

---

## Implementasi — 3 fase

---

## Phase 1 — Dynamic SimDisplay Resolution

### 1.1 SimDisplay: ubah static const menjadi instance members

**`sim_display.h` sekarang:**
```cpp
static constexpr uint16_t W = 264;
static constexpr uint16_t H = 176;
uint8_t buf_[W * H] = {};
```

**`sim_display.h` setelah:**
```cpp
uint16_t w_ = 264;
uint16_t h_ = 176;
uint8_t* buf_ = nullptr;  // heap-allocated in init()
```

Seluruh referensi `W` dan `H` di impl diganti `w_` dan `h_`.

### 1.2 Baca dimensi dari environment variable

Di `SimDisplay::init()`:
```cpp
void SimDisplay::init(Logger& log, TelemetryBridge& bridge) {
    log_    = &log;
    bridge_ = &bridge;

    const char* envW = std::getenv("KAIRO_SIM_W");
    const char* envH = std::getenv("KAIRO_SIM_H");
    w_ = envW ? (uint16_t)std::atoi(envW) : 264;
    h_ = envH ? (uint16_t)std::atoi(envH) : 176;

    // Clamp to sane range
    if (w_ < 64)  w_ = 64;
    if (h_ < 32)  h_ = 32;
    if (w_ > 1024) w_ = 1024;
    if (h_ > 768)  h_ = 768;

    buf_ = new uint8_t[(size_t)w_ * h_]();
}
```

Tambah `stop()` untuk `delete[] buf_`.

### 1.3 `index.ts` — pass resolusi ke binary

Simpan resolusi aktif di server state:
```ts
let simResolution = { w: 264, h: 176 };

function bootSim() {
    proc = Bun.spawn([BIN_PATH], {
        env: {
            ...process.env,
            KAIRO_SIM_JSON: "1",
            KAIRO_SIM_W: String(simResolution.w),
            KAIRO_SIM_H: String(simResolution.h),
        },
        // ...
    });
}
```

Handle command `set_resolution` — simpan ke `simResolution` dan restart:
```ts
if (cmd === "set_resolution") {
    simResolution = { w: Number(msg.w) || 264, h: Number(msg.h) || 176 };
    // shutdown existing, boot new
    if (proc) sendToSim({ cmd: "shutdown" });
    setTimeout(bootSim, 200);
}
```

### 1.4 `useSimSocket.ts` — track resolusi aktif

Tambah `resolution: { w: number; h: number }` ke `SimState`.
Update saat `frame` message masuk (frame sudah punya `width` / `height`):
```ts
if (type === "frame") {
    return { ...prev, frame, resolution: { w: frame.width, h: frame.height } };
}
```

### 1.5 `DisplayPanel` — dynamic dimensions

Hapus `const W = 264, H = 176` hardcoded. Pakai dari props:
```tsx
export function DisplayPanel({
    frame, displaySleeping, resolution
}: {
    frame: FrameMsg | null;
    displaySleeping: boolean;
    resolution: { w: number; h: number };
}) {
    const { w, h } = resolution;
    const SCALE = Math.max(1, Math.floor(528 / w));  // fit in ~528px wide
    // canvas width={w} height={h}, styled width={w*SCALE} height={h*SCALE}
```

Header menampilkan resolusi aktual: `${w}×${h} · 1-bit`.

---

## Phase 2 — Frontend UI Redesign

### 2.1 Komponen baru

**`HardwareButtons.tsx`** — D-pad + action buttons:
```tsx
// D-pad: CSS Grid 3×3, 40×40px per cell, 6px gap
// Cell positions: UP=(0,1), LEFT=(1,0), RIGHT=(1,2), DOWN=(2,1)
// Action: flex-col, SELECT=green circle, CANCEL=red circle

export function HardwareButtons({ onKey, running }) { ... }
```

**`SettingsTabs.tsx`** — tabbed settings dengan 3 tab:
- `WiFiTab` — extracted dari ControlsPanel
- `DisplayTab` — resolution picker + theme
- `SystemTab` — boot/shutdown/restart + inject event

**`DevicePanel.tsx`** — wrapper kiri:
- Atas: `<DisplayPanel>`
- Bawah: `<HardwareButtons>`

### 2.2 Layout `frontend.tsx` — 3 kolom

```tsx
// Ganti grid "570px 1fr" dengan:
grid: {
    display: "grid",
    gridTemplateColumns: "auto 320px 1fr",
    //                    ^device ^settings ^logs
}
```

Lebar device panel = display width × scale + padding, **auto** (mengikuti resolusi).
Settings panel = 320px fixed.
Logs panel = sisa.

### 2.3 `DisplayTab` — Resolution Picker

```tsx
const PRESETS = [
    { label: "2.7\" E-ink ★", w: 264, h: 176 },  // default
    { label: "1.54\" E-ink",  w: 200, h: 200 },
    { label: "2.13\" E-ink",  w: 250, h: 122 },
    { label: "2.9\" E-ink",   w: 296, h: 128 },
    { label: "4.2\" E-ink",   w: 400, h: 300 },
    { label: "5.83\" E-ink",  w: 640, h: 384 },
    { label: "7.5\" E-ink",   w: 800, h: 480 },
    { label: "0.96\" OLED",   w: 128, h: 64  },
    { label: "0.91\" OLED",   w: 128, h: 32  },
    { label: "Custom",        w: 0,   h: 0   },
];
```

UI:
- Dropdown list preset di atas
- Kalau "Custom" dipilih: input W dan H muncul
- Tombol "Apply & Restart" — disabled kalau sim tidak running
- Warning: "Applying will restart the simulator"

### 2.4 `SystemTab` — Runtime controls

Pindahkan dari Device panel ke System tab:
```
RUNTIME
[  Boot  ]  [ Shutdown ]  [ Restart ]

INJECT EVENT
[  EventName  ] [ Inject ]
```

### 2.5 `LogsPanel` restructure

Split menjadi 3 sub-sections yang bisa collapse:
- **Services** (selalu di atas, compact)
- **Logs** (filter: TRACE/DEBUG/INFO/WARN/ERROR/FATAL tetap ada)
- **Events** (di bawah)

---

## Phase 3 — Polish & Detail

### 3.1 Device bezel / frame

Display dibungkus dalam `div` dengan border-radius dan subtle shadow yang
meniru bezel perangkat fisik. Warna bezel = dark gray (`#1e1e1e`).

```tsx
<div style={styles.deviceBezel}>
    <DisplayPanel ... />
</div>
```

### 3.2 Button visual improvements

D-pad dan action buttons pakai style yang terasa seperti tombol fisik:
- `box-shadow` untuk raised effect
- `:active` → `translateY(1px)` untuk press feedback
- Icons: `▲ ▼ ◄ ►` di D-pad, `●` dan `✕` di action buttons

### 3.3 Scale indicator di Display tab

Setelah Plan 25 Phase 2 diterapkan, tampilkan:
```
Resolution: 264×176
Scale factor: 1× (logical = 264×176)

Resolution: 528×352
Scale factor: 2× (logical = 264×176)
```

### 3.4 Connection status header

Header bar yang sudah ada — tambahkan info resolusi aktif:
```
Kairo Simulator  ● running   simulator/simulator · vdev   264×176
```

---

## File yang berubah

### Firmware (C++)
| File | Perubahan |
|------|-----------|
| `platforms/simulator/include/kairo/sim/sim_display.h` | W/H jadi instance members, buf_ jadi pointer |
| `platforms/simulator/src/sim_display.cpp` | Baca env vars, dynamic alloc, ganti semua W→w_, H→h_ |

### Server (TypeScript)
| File | Perubahan |
|------|-----------|
| `packages/simulator/index.ts` | simResolution state, pass env vars ke spawn, handle set_resolution |

### Frontend (React/TSX)
| File | Aksi |
|------|------|
| `packages/simulator/frontend.tsx` | Layout 3 kolom, wire komponen baru |
| `packages/simulator/components/DisplayPanel.tsx` | Dynamic W/H dari props, SCALE computed |
| `packages/simulator/components/HardwareButtons.tsx` | **Baru** — D-pad + SELECT/CANCEL |
| `packages/simulator/components/DevicePanel.tsx` | **Baru** — display + buttons wrapper |
| `packages/simulator/components/SettingsTabs.tsx` | **Baru** — WiFi \| Display \| System tabs |
| `packages/simulator/components/WiFiTab.tsx` | **Baru** — extracted dari ControlsPanel |
| `packages/simulator/components/DisplayTab.tsx` | **Baru** — resolution picker + theme |
| `packages/simulator/components/SystemTab.tsx` | **Baru** — boot/shutdown/restart/inject |
| `packages/simulator/components/ControlsPanel.tsx` | **Hapus** — diganti SettingsTabs |
| `packages/simulator/lib/useSimSocket.ts` | Tambah `resolution` ke SimState |

---

## Urutan implementasi

1. Phase 1 dulu — SimDisplay dynamic, server pass env, DisplayPanel dynamic
2. Verifikasi: ganti resolusi → simulator restart → tampil resolusi baru di canvas
3. Phase 2 — komponen baru, layout 3 kolom, settings tabs
4. Phase 3 — polish bezel, button feel, scale indicator

---

## Acceptance criteria

- [ ] Pilih "4.2\" E-ink (400×300)" di Display tab → Apply → sim restart → display tumbuh
- [ ] Pilih "0.96\" OLED (128×64)" → display mengecil, layout tetap rapi
- [ ] Custom 320×240 berfungsi
- [ ] Tombol D-pad visual seperti cross, SELECT/CANCEL di sebelah kanan
- [ ] WiFi konfigurasi ada di tab WiFi, tidak campur dengan tombol
- [ ] Boot/Shutdown/Restart ada di tab System
- [ ] Pada resolusi apapun: `frame.width` dan `frame.height` di JSON match resolusi yang dipilih
- [ ] Scale indicator tampil benar (setelah Plan 25 Phase 2)
- [ ] Simulator & ESP32 builds clean

---

## Non-Goals

- Landscape/portrait toggle di simulator (plan terpisah)
- Color display simulation (Kairo is 1-bit only saat ini)
- Recording/replay session
- Multiple display panels sekaligus
- Mobile-responsive simulator (desktop-only tool)
