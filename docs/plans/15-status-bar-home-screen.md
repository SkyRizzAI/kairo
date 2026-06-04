# 15 — Status Bar & Home Screen (Retro Style)

> Status bar 1 baris di atas (clock, wifi, battery), Home Screen dengan ASCII art + app launcher. Desain mengikuti Flipper Zero / e-ink aesthetic: compact, pixel, minimalist.

- Status: ☐ Not started
- Milestone: M5 (UI Runtime)
- Depends on: 12 (plugin), 13 (canvas), 14 (ViewDispatcher, IScreen, Key)
- Blocks: — (end of M5)

---

## Goal

```
┌────────────────────────────────────────────┐  ← border frame
│ 12:34  ▒▒▒▒░  W              KAIRO v0.1   │  ← status bar (y=0..8)
├────────────────────────────────────────────┤  ← separator line
│  ██╗ ██╗  █████╗ ██╗██████╗  ██████╗      │
│  ██║ ██║ ██╔══██╗██║██╔══██╗██╔═══██╗     │  ← ASCII art logo
│  █████║ ███████║██║██████╔╝██║   ██║     │
│  ██╔══╝ ██╔══██║██║██╔══██╗██║   ██║     │
│  ██║    ██║  ██║██║██║  ██║╚██████╔╝     │
│                                            │
│  > Hello Plugin                            │  ← app list (cursor = ">")
│    Settings                                │
│    WiFi Scanner                            │
│    About                                   │
│                                            │
├────────────────────────────────────────────┤
│  OK=open  BACK=power off                   │  ← footer hints
└────────────────────────────────────────────┘
```

- Refresh hanya saat navigasi (Up/Down/Ok/Back).
- Semua diimplementasi sebagai plugin via PluginContext.

## Scope

### In scope

- `StatusBar` struct: draw satu baris di y=0, dipanggil tiap `draw()` dari screen mana pun.
- `HomeScreen : IScreen` — ASCII art KAIRO + app list + cursor.
- Navigasi Up/Down scroll list, Ok = launch app, Back = nothing (home is base).
- Battery indicator: blok ASCII (▓▓▓▓░ = 80%).
- WiFi indicator: `W` jika connected, `-` jika tidak.
- Clock: `HH:MM` dari EventBus (subscribe ClockTick) atau read dari `rt.clock().epochMs()`.
- Footer hints: teks kecil di y=168.

### Out of scope

- App launch nyata (push screen plugin yang punya screen sendiri — diisi nanti).
- Settings screen (post-MVP).
- Animasi cursor / scroll.

---

## Design

### StatusBar (standalone helper, bukan plugin)

StatusBar bukan plugin terpisah — lebih simpel sebagai helper yang di-call dari `draw()` tiap screen.

```cpp
// firmware/core/include/kairo/ui/status_bar.h
namespace kairo {
struct StatusBarData {
    int      hour, minute;
    int      batteryPct;  // 0–100
    bool     wifiConnected;
    const char* version = "v0.1";
};

class StatusBar {
public:
    // Render 1 baris di atas canvas + garis separator
    static void draw(Canvas& c, const StatusBarData& d);
    // Battery indicator: ▓▓▓▓░ (5 blocks, 1 = 20%)
    static void drawBattery(Canvas& c, uint16_t x, uint16_t y, int pct);
};
}
```

Setiap screen yang ingin punya status bar cukup panggil:
```cpp
void MyScreen::draw(Canvas& c) {
    c.clear();
    StatusBar::draw(c, statusData_);
    // ... konten screen di bawahnya
}
```

### HomeScreen

```cpp
class HomeScreen : public IScreen {
public:
    HomeScreen(Runtime& rt);

    void enter() override;
    void update(Key key) override;
    void draw(Canvas& canvas) override;
    void tick(uint64_t nowMs) override;  // update clock tiap menit

private:
    Runtime&    rt_;
    int         cursor_  = 0;
    int         scroll_  = 0;
    StatusBarData status_ = {};

    struct AppEntry { const char* name; IScreen* screen; };
    std::vector<AppEntry> apps_;

    void buildAppList();     // populate dari plugin manager
    void launchSelected();   // push apps_[cursor_].screen ke ViewDispatcher
    void drawLogo(Canvas& c, uint16_t y);
    void drawAppList(Canvas& c, uint16_t y);
    void drawFooter(Canvas& c);
};
```

### ASCII art logo "KAIRO"

Dibuat dengan font ASCII besar (manual atau dari figlet). Contoh 5 baris × 32 kolom:

```
 _  __   _   ___ ____   ___
| |/ /  / \ |_ _|  _ \ / _ \
| ' /  / _ \ | || |_) | | | |
| . \ / ___ \| ||  _ <| |_| |
|_|\_/_/   \_|___|_| \_\\___/
```

Di-render mulai y=`CONTENT_Y + 2`, x=`(SCREEN_W - strlen(line)*CHAR_W) / 2` untuk centering.

### App list rendering

```
> Hello Plugin      ← cursor = >
  Settings
  WiFi Scanner
  About
```

- Cursor `>` di kolom 2, nama app mulai kolom 4.
- Visible window: `CONTENT_ROWS - 7` (atas: logo ~6 baris + 1 spasi) = ~10 baris untuk list.
- Scroll: `scroll_` offset saat cursor melebihi visible window.
- `invertRect` pada baris cursor untuk highlight bar hitam.

```cpp
void HomeScreen::drawAppList(Canvas& c, uint16_t startY) {
    uint16_t y = startY;
    for (int i = scroll_; i < (int)apps_.size() && y < ui::FOOTER_Y - ui::CHAR_H; i++) {
        bool selected = (i == cursor_);
        if (selected) c.invertRect(0, y-1, c.width(), ui::CHAR_H+1);
        c.drawText(selected ? 2 : 4, y, apps_[i].name, !selected);
        y += ui::CHAR_H;
    }
}
```

### Key handling

```cpp
void HomeScreen::update(Key key) {
    switch (key) {
        case Key::Up:
            if (cursor_ > 0) { cursor_--; if (cursor_ < scroll_) scroll_--; }
            break;
        case Key::Down:
            if (cursor_ < (int)apps_.size()-1) { cursor_++; if (cursor_ >= scroll_ + VISIBLE) scroll_++; }
            break;
        case Key::Ok:
            launchSelected();
            break;
        default: break;
    }
    rt_.view().requestRedraw();
}
```

### StatusBarData update

HomeScreen subscribe ke EventBus:
- `ClockTick` → update `status_.hour/minute`, requestRedraw (clock update tiap detik).
- `BatteryChanged` → update `status_.batteryPct`.
- `NetworkConnected/Disconnected` → update `status_.wifiConnected`.

### main.cpp

```cpp
// Setelah rt.start():
static HomeScreen homeScreen(rt);
rt.view().push(homeScreen);
rt.view().requestRedraw();
```

---

## Tasks

- [ ] `StatusBar::draw()` + `drawBattery()`.
- [ ] `HomeScreen` (enter/update/draw/tick + buildAppList).
- [ ] ASCII art KAIRO di `draw()` (hardcoded string array, centering).
- [ ] App list render dengan `invertRect` cursor highlight.
- [ ] Key navigation (Up/Down scroll, Ok = push screen).
- [ ] EventBus subscriptions di `enter()` (ClockTick, BatteryChanged, Network*).
- [ ] `main.cpp`: push HomeScreen ke ViewDispatcher.
- [ ] `buildAppList()`: populate dari `rt.plugins().plugins()` — pakai nama plugin.
- [ ] Verifikasi: boot → HomeScreen tampil, D-pad web bisa navigasi list.

## Acceptance criteria

- Status bar menampilkan jam, battery indicator, wifi status.
- Battery blok update saat `BatteryChanged` event.
- Clock update saat `ClockTick`.
- Up/Down navigasi list, cursor highlight berpindah.
- Ok pada item → log "launching [name]" (screen push nanti saat plugin punya screen).
- `invertRect` pada cursor baris terlihat jelas (warna terbalik).
- Layout dalam 264×176 tidak overflow — semua elemen fit.

## Visual spec ringkas

```
[0  ] Status bar:  "HH:MM ▓▓▓░░ W          KAIRO v0.1"
[9  ] Separator:   horizontal line full width
[10 ] Logo:        ASCII art "KAIRO" ~5 baris, centered
[55 ] Spacer:      1 baris kosong
[64 ] App list:    ">" + nama, ~10 item visible, invertRect cursor
[168] Separator:   horizontal line full width
[169] Footer:      "  OK=open  BACK=off  ↑↓=navigate"
[176] end
```

## Risks / notes

- ASCII art logo lebarnya ~32–38 char → fit di 44 kolom. Cek di simulator sebelum ke hardware.
- `buildAppList()` dipanggil di `enter()` — list otomatis update saat screen di-push ulang (setelah plugin baru di-load).
- Tidak ada animation / transition — setiap key press langsung redraw. Sesuai e-ink constraint.
- `statusData_` diupdate dari event subscriptions — jangan lupa unsubscribe saat plugin unload (PluginContext handle ini otomatis di plan 12).
