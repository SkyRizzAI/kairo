// Mission Control — Flipper-style quick-settings panel (Plan 92 — Control Center).
#include "nema/screens/mission_control_screen.h"
#include "nema/runtime.h"
#include "nema/task_runner.h"
#include "nema/ui/canvas.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/icon_pack.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include "nema/services/audio_service.h"
#include "nema/services/display_power_manager.h"
#include "nema/hal/audio_output.h"
#include "nema/hal/wifi.h"
#include "nema/hal/display.h"
#include "nema/assets/system_icons.h"

namespace nema {

using namespace aether::ui;

// ── Tile icons (hand-coded 8×8, 1 byte/row, MSB = leftmost). Easily tweakable. ──
static const uint8_t kIcDark[8]    = {0x3C, 0x72, 0xF1, 0xF1, 0xF1, 0xF1, 0x72, 0x3C}; // contrast (dark mode)
static const uint8_t kIcRestart[8] = {0x3C, 0x42, 0x81, 0x87, 0x82, 0x80, 0x42, 0x3C}; // refresh ring + arrow
static const uint8_t kIcSun[8]     = {0x18, 0x5A, 0x3C, 0xFF, 0xFF, 0x3C, 0x5A, 0x18}; // brightness
static const uint8_t kIcSpk[8]     = {0x10, 0x32, 0xF5, 0xF5, 0xF5, 0xF5, 0x32, 0x10}; // volume

MissionControlScreen::MissionControlScreen(Runtime& rt)
    : ComponentScreen(rt), settings_(rt), wifiSettings_(rt),
      restartSplash_(rt, SplashScreen::Mode::Restart) {}

void MissionControlScreen::onResume() {
    brightness_ = rt_.canvas().driver().brightness();
    volume_     = (int)rt_.config().getIntOr("aether", "volume", 100);
    darkOn_     = aether::darkMode();
    if (auto* d = rt_.container().resolve<IWifiDriver>()) wifiOn_ = d->isEnabled();
    state_.focus.focused = 0;
    requestRedraw();
}

// Focus order (document order): 0 dark · 1 wifi · 2 lock · 3 settings · 4 restart
// · 5 brightness bar · 6 volume bar. The bars appear in every grid row so Left/Right
// reach them; on a bar, Up/Down adjusts instead of moving.
void MissionControlScreen::activate(int f) {
    switch (f) {
        case 0: onDark(this);     break;
        case 1: onWifi(this);     break;
        case 2: onLock(this);     break;
        case 3: onSettings(this); break;
        case 4: onRestart(this);  break;
        default: break;   // bars have no activate
    }
}

void MissionControlScreen::adjustBar(int f, int dir) {
    if (f == 5) {
        int v = brightness_ + dir * 16;
        brightness_ = v < 0 ? 0 : (v > 255 ? 255 : v);
        onBrightness(this, brightness_);
    } else if (f == 6) {
        int v = volume_ + dir * 5;
        volume_ = v < 0 ? 0 : (v > 100 ? 100 : v);
        onVolume(this, volume_);
    }
    requestRedraw();
}

void MissionControlScreen::onAction(input::Action a) {
    using A = input::Action;
    state_.modality = input::InputModality::Button;
    if (a == A::Back) { rt_.view().goBack(); return; }

    int f = state_.focus.focused;
    if (a == A::Activate) { activate(f); return; }

    bool bar = (f == 5 || f == 6);
    if (bar && (a == A::Prev || a == A::Next)) {     // Up/Down on a bar → adjust
        adjustBar(f, a == A::Prev ? +1 : -1);
        return;
    }

    // 2D grid (bars repeated each row). Left=AdjustDown, Right=AdjustUp, Up=Prev, Down=Next.
    static const int gLand[2][5] = {{0, 1, 2, 5, 6}, {3, 4, -1, 5, 6}};
    static const int gPort[3][4] = {{0, 1, 5, 6}, {2, 3, 5, 6}, {4, -1, 5, 6}};
    bool ls = rt_.canvas().width() > rt_.canvas().height();
    int rows = ls ? 2 : 3, cols = ls ? 5 : 4;
    auto cell = [&](int r, int c) -> int { return ls ? gLand[r][c] : gPort[r][c]; };

    int cr = 0, cc = 0; bool got = false;
    for (int r = 0; r < rows && !got; r++)
        for (int c = 0; c < cols; c++)
            if (cell(r, c) == f) { cr = r; cc = c; got = true; break; }

    int nf = f;
    if (a == A::AdjustUp)        { for (int c = cc + 1; c < cols; c++) if (cell(cr, c) >= 0) { nf = cell(cr, c); break; } }
    else if (a == A::AdjustDown) { for (int c = cc - 1; c >= 0; c--)   if (cell(cr, c) >= 0) { nf = cell(cr, c); break; } }
    else if (a == A::Prev)       { for (int r = cr - 1; r >= 0; r--)   if (cell(r, cc) >= 0) { nf = cell(r, cc); break; } }
    else if (a == A::Next)       { for (int r = cr + 1; r < rows; r++) if (cell(r, cc) >= 0) { nf = cell(r, cc); break; } }
    if (nf != f) { state_.focus.focused = nf; requestRedraw(); }
}

UiNode* MissionControlScreen::tile(NodeArena& a, const uint8_t* ic, uint8_t iw, uint8_t ih,
                                   void (*press)(void*)) {
    Style s;
    s.border  = true;
    s.align   = Align::Center;
    s.justify = Justify::Center;
    s.cornerRadius = 6;   // visibly rounded square (size set by the caller)
    UiNode* ico = Icon(a, ic, iw, ih);
    if (ico) ico->iconScale = 3;   // 8×8 → 24×24, readable in the big tiles
    return Pressable(a, press, this, s, { ico });
}

UiNode* MissionControlScreen::build(NodeArena& a, Runtime& rt) {
    const IconDef* gear = findIcon("feature.settings");

    UiNode* dark = tile(a, kIcDark, 8, 8, onDark);
    UiNode* wifi = tile(a, assets::icWifiOn.data, assets::icWifiOn.w, assets::icWifiOn.h, onWifi);
    UiNode* lock = tile(a, assets::icLock.data, assets::icLock.w, assets::icLock.h, onLock);
    UiNode* setg = tile(a, gear ? gear->bitmap : kIcRestart,
                        gear ? gear->w : 8, gear ? gear->h : 8, onSettings);
    UiNode* rest = tile(a, kIcRestart, 8, 8, onRestart);

    // ── Fit/centre layout: precise SQUARE tiles + two bars matched to the grid ──
    uint16_t W = rt.canvas().width(), H = rt.canvas().height();
    bool ls = W > H;
    const int tcols = ls ? 3 : 2, trows = ls ? 2 : 3;
    const int gap = 6, pad = 10;
    // Bar width tracks the tile size (barW = ts·2/5) so the ratio holds as the screen
    // shrinks. Solve the square side from the horizontal budget with that folded in:
    //   W - 2pad = tcols·ts + (tcols+1)·gap + 2·(ts·2/5)  → ts = 5·budget / (5·tcols+4)
    int tsW = (5 * ((int)W - pad * 2 - (tcols + 1) * gap)) / (5 * tcols + 4);
    int tsH = ((int)H - pad * 2 - (trows - 1) * gap) / trows;
    int ts  = tsW < tsH ? tsW : tsH;                     // square side
    if (ts < 12) ts = 12;
    int barW = ts * 2 / 5;
    if (barW < 8) barW = 8;
    int gridH = trows * ts + (trows - 1) * gap;

    UiNode* tiles[5] = { dark, wifi, lock, setg, rest };
    for (UiNode* t : tiles) if (t) {
        t->style.width = (uint16_t)ts; t->style.height = (uint16_t)ts;
        // Icon auto-scales with the square (~half the tile), shrinking when it shrinks.
        if (UiNode* ic = t->firstChild) {
            uint8_t nat = ic->iconW > ic->iconH ? ic->iconW : ic->iconH;
            int sc = (ts / 2) / (nat ? nat : 8);
            ic->iconScale = (uint8_t)(sc < 1 ? 1 : sc);
        }
    }
    auto spacer = [&] { Style e; e.width = (uint16_t)ts; e.height = (uint16_t)ts; return View(a, e, {}); };

    Style colS; colS.dir = FlexDir::Col; colS.gap = gap;
    Style rowS; rowS.dir = FlexDir::Row; rowS.gap = gap;
    UiNode* grid = ls
        ? Col(a, colS, { Row(a, rowS, { dark, wifi, lock }),
                         Row(a, rowS, { setg, rest, spacer() }) })
        : Col(a, colS, { Row(a, rowS, { dark, wifi }),
                         Row(a, rowS, { lock, setg }),
                         Row(a, rowS, { rest, spacer() }) });

    // Brightness + volume — pill bars sized to the GRID height (not the screen),
    // with the glyph overlaid in the centre.
    uint8_t barR = (uint8_t)(barW / 3 < 2 ? 2 : barW / 3);   // pill radius scales with width
    UiNode* bs = Slider(a, &brightness_, 0, 255, 16, onBrightness, this);
    bs->sliderVertical = true; bs->style.width = (uint16_t)barW; bs->style.height = (uint16_t)gridH;
    bs->style.cornerRadius = barR; bs->iconBitmap = kIcSun; bs->iconW = 8; bs->iconH = 8;
    UiNode* vs = Slider(a, &volume_, 0, 100, 5, onVolume, this);
    vs->sliderVertical = true; vs->style.width = (uint16_t)barW; vs->style.height = (uint16_t)gridH;
    vs->style.cornerRadius = barR; vs->iconBitmap = kIcSpk; vs->iconW = 8; vs->iconH = 8;

    Style root; root.dir = FlexDir::Row; root.flexGrow = 1; root.padding = pad;
    root.align = Align::Center; root.justify = Justify::Center; root.gap = gap;
    return View(a, root, { grid, bs, vs });
}

// ── Tile actions ───────────────────────────────────────────────────────────
void MissionControlScreen::onDark(void* u) {
    auto* s = static_cast<MissionControlScreen*>(u);
    s->darkOn_ = !s->darkOn_;
    aether::setDarkMode(s->darkOn_);
    s->rt_.config().setInt("aether", "dark", s->darkOn_ ? 1 : 0);
    s->requestRedraw();
}

void MissionControlScreen::onWifi(void* u) {
    auto* s = static_cast<MissionControlScreen*>(u);
    IWifiDriver* d = s->rt_.container().resolve<IWifiDriver>();
    if (!d) return;
    bool on = !d->isEnabled();
    s->wifiOn_ = on;
    if (on && d->savedCount() == 0) {
        // Nothing saved to auto-join — turn the radio on and open WiFi settings so
        // the user can pick/add a network.
        s->rt_.tasks().submit([d] { d->setEnabled(true); d->scan(); }, [s] { s->requestRedraw(); });
        s->rt_.view().navigate(s->wifiSettings_);
        return;
    }
    // Blocking radio work runs on a worker; on enable, auto-join the strongest saved net.
    s->rt_.tasks().submit([d, on] { d->setEnabled(on); if (on) { d->scan(); d->autoConnect(); } },
                          [s] { s->requestRedraw(); });
    s->requestRedraw();
}

void MissionControlScreen::onLock(void* u) {
    auto* s = static_cast<MissionControlScreen*>(u);
    s->rt_.view().goBack();          // pop Mission Control back to the desktop first
    s->rt_.dpm().lockNow();
}

void MissionControlScreen::onSettings(void* u) {
    auto* s = static_cast<MissionControlScreen*>(u);
    s->rt_.view().navigate(s->settings_);
}

void MissionControlScreen::onRestart(void* u) {
    auto* s = static_cast<MissionControlScreen*>(u);
    // Show the splash (2s) first, then it calls requestRestart itself (aether-side,
    // no kernel hook). CLI/remote reboots are kernel paths and skip the splash.
    s->rt_.view().navigate(s->restartSplash_);
}

void MissionControlScreen::onBrightness(void* u, int v) {
    auto* s = static_cast<MissionControlScreen*>(u);
    s->rt_.canvas().driver().setBrightness((uint8_t)v);
    s->rt_.config().setInt("display", "brightness", v);
}

void MissionControlScreen::onVolume(void* u, int v) {
    auto* s = static_cast<MissionControlScreen*>(u);
    float g = (float)v / 100.0f;
    AudioService& au = s->rt_.audio();
    for (int i = 0; i < au.outputCount(); i++)
        if (auto* o = au.output(i)) o->setVolume(g);
    s->rt_.config().setInt("aether", "volume", v);
}

} // namespace nema
