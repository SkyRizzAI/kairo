#include "kairo/screens/sounds_settings_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/components.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/audio_service.h"
#include "kairo/input/input_action.h"
#include <cstdio>

namespace kairo {

SoundsSettingsScreen::SoundsSettingsScreen(Runtime& rt) : rt_(rt) {}

void SoundsSettingsScreen::enter() {
    cursor_ = 0;
    rt_.view().requestRedraw();
}

void SoundsSettingsScreen::tick(uint64_t /*nowMs*/) {
    rt_.view().requestRedraw();
}

void SoundsSettingsScreen::update(Key key) {
    int total = rt_.audio().inputCount() + rt_.audio().outputCount();
    if (rt_.audio().outputCount() > 0) total++;  // +1 for Test row

    switch (key) {
    case Key::Up:
        if (cursor_ > 0) cursor_--;
        break;
    case Key::Down:
        if (cursor_ < total - 1) cursor_++;
        break;
    case Key::Select: {
        int testRow = rt_.audio().inputCount() + rt_.audio().outputCount();
        if (cursor_ == testRow && rt_.audio().outputCount() > 0)
            rt_.audio().output(0)->playTone(440, 300);
        break;
    }
    case Key::Cancel:
        rt_.view().pop();
        return;
    default:
        break;
    }
    rt_.view().requestRedraw();
}

// Draw a device level row:
//
//   LABEL     0 [████░░░░] 42 /100
//
// Layout (240px canvas):
//   col 5..89  = label (14 chars max)
//   col 90     = "0"
//   col 96..175 = bar (80px)
//   col 177    = current %
//   col 207    = "/100"
void SoundsSettingsScreen::drawDeviceRow(Canvas& c, uint16_t y, bool sel,
                                          const char* label, float level) const {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    int pct = (int)(level * 100.0f + 0.5f);

    if (sel) c.invertRect(2, (uint16_t)(y - 1),
                          (uint16_t)(c.width() - 4), (uint16_t)(ui::CHAR_H + 1));
    bool ink = !sel;

    // Label (truncated to 12 chars)
    char lbuf[16];
    std::snprintf(lbuf, sizeof(lbuf), "%-12.12s", label);
    c.drawText(5, y, lbuf, ink);

    // "0" min label
    c.drawText(77, y, "0", ink);

    // Level bar (80px wide)
    constexpr uint16_t BAR_X = 89;
    constexpr uint16_t BAR_W = 80;
    uint16_t fill = (uint16_t)(BAR_W * level);
    if (fill > 0)        c.fillRect(BAR_X,         y, fill,          ui::CHAR_H, ink);
    if (fill < BAR_W)    c.fillRect(BAR_X + fill,  y, BAR_W - fill,  ui::CHAR_H, !ink);

    // Current value
    char cur[8];
    std::snprintf(cur, sizeof(cur), "%3d", pct);
    c.drawText(174, y, cur, ink);

    // Max label
    c.drawText(201, y, "/100", ink);
}

void SoundsSettingsScreen::draw(Canvas& c) {
    uint16_t y = ui::drawTitle(c, "SOUNDS");

    // INPUT section
    c.drawText(5, y, "INPUT", true);
    y += ui::CHAR_H + 2;

    if (rt_.audio().inputCount() == 0) {
        c.drawText(5, y, "No input devices", true);
        y += ui::CHAR_H + 4;
    } else {
        for (int i = 0; i < rt_.audio().inputCount(); i++) {
            bool sel = (cursor_ == i);
            auto* dev = rt_.audio().input(i);
            drawDeviceRow(c, y, sel, dev->label(), dev->peakLevel());
            y += ui::CHAR_H + 6;
        }
    }

    y += 2;

    // OUTPUT section
    c.drawText(5, y, "OUTPUT", true);
    y += ui::CHAR_H + 2;

    if (rt_.audio().outputCount() == 0) {
        c.drawText(5, y, "No output devices", true);
        y += ui::CHAR_H + 4;
    } else {
        for (int i = 0; i < rt_.audio().outputCount(); i++) {
            int row = rt_.audio().inputCount() + i;
            bool sel = (cursor_ == row);
            auto* dev = rt_.audio().output(i);
            drawDeviceRow(c, y, sel, dev->label(), dev->peakLevel());
            y += ui::CHAR_H + 6;
        }
    }

    // Test beep row
    if (rt_.audio().outputCount() > 0) {
        y += 2;
        int testRow = rt_.audio().inputCount() + rt_.audio().outputCount();
        bool sel = (cursor_ == testRow);
        if (sel) c.invertRect(2, (uint16_t)(y - 1),
                              (uint16_t)(c.width() - 4), (uint16_t)(ui::CHAR_H + 1));
        c.drawText(5, y, "[ Test Beep 440Hz ]", !sel);
    }

    // Footer
    char footer[56];
    std::snprintf(footer, sizeof(footer), "%s back",
        rt_.input().hintFor(input::Action::Back));
    c.drawText(4, ui::footerY(c.height()), footer, true);
}

} // namespace kairo
