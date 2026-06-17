#include "nema/screens/sounds_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/audio_service.h"
#include <cstdio>

namespace nema {

using namespace ui;

SoundsSettingsScreen::SoundsSettingsScreen(Runtime& rt) : ComponentScreen(rt, 96) {}

void SoundsSettingsScreen::enter() {
    scroll_.scrollMain = 0;
    rt_.view().requestRedraw();
}

void SoundsSettingsScreen::tick(uint64_t) {
    rt_.view().requestRedraw();   // live level meters
}

void SoundsSettingsScreen::onTestBeep(void* u) {
    auto* s = static_cast<SoundsSettingsScreen*>(u);
    if (s->rt_.audio().outputCount() > 0)
        s->rt_.audio().output(0)->playTone(440, 300);
}

// "label  [####----] 42%" — 8-segment text meter.
static void formatBar(std::vector<std::string>& out, const char* label, float level) {
    if (level < 0) level = 0;
    if (level > 1) level = 1;
    char buf[48];
    char bar[9];
    int fill = (int)(level * 8 + 0.5f);
    for (int i = 0; i < 8; i++) bar[i] = i < fill ? '#' : '-';
    bar[8] = '\0';
    std::snprintf(buf, sizeof(buf), "%-10.10s [%s] %d%%", label, bar, (int)(level * 100 + 0.5f));
    out.push_back(buf);
}

UiNode* SoundsSettingsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    auto& audio = rt.audio();
    rows_.push_back("INPUT");
    if (audio.inputCount() == 0) rows_.push_back("  (none)");
    for (int i = 0; i < audio.inputCount(); i++)
        formatBar(rows_, audio.input(i)->label(), audio.input(i)->peakLevel());
    rows_.push_back("");
    rows_.push_back("OUTPUT");
    if (audio.outputCount() == 0) rows_.push_back("  (none)");
    for (int i = 0; i < audio.outputCount(); i++)
        formatBar(rows_, audio.output(i)->label(), audio.output(i)->peakLevel());

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    Style line; line.height = 1; line.background = true;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 1;

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };
    for (auto& r : rows_) append(Text(a, r.c_str(), TextRole::Body));
    if (audio.outputCount() > 0) append(ListRow(a, "Test Beep 440Hz", onTestBeep, this));

    return View(a, root, {
        Text(a, "SOUNDS", TextRole::Title),
        View(a, line, {}),
        list,
    });
}

} // namespace nema
