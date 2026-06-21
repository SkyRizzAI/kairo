#include "nema/screens/sounds_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/audio_service.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

SoundsSettingsScreen::SoundsSettingsScreen(Runtime& rt) : ComponentScreen(rt, 96) {}

void SoundsSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

void SoundsSettingsScreen::tick(uint64_t) {
    rt_.view().requestRedraw();
}

void SoundsSettingsScreen::onTestBeep(void* u) {
    auto* s = static_cast<SoundsSettingsScreen*>(u);
    if (s->rt_.audio().outputCount() > 0)
        s->rt_.audio().output(0)->playTone(440, 300);
}

static void formatMeterVal(char* buf, size_t sz, float level) {
    if (level < 0) level = 0;
    if (level > 1) level = 1;
    char bar[9];
    int fill = (int)(level * 8 + 0.5f);
    for (int i = 0; i < 8; i++) bar[i] = i < fill ? '#' : '-';
    bar[8] = '\0';
    std::snprintf(buf, sz, "[%s] %d%%", bar, (int)(level * 100 + 0.5f));
}

UiNode* SoundsSettingsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    auto& audio = rt.audio();

    // Pre-format all meter value strings before building nodes (prevents
    // vector reallocation from invalidating earlier .c_str() pointers).
    rows_.reserve((size_t)(audio.inputCount() + audio.outputCount()));
    for (int i = 0; i < audio.inputCount(); i++) {
        char buf[20];
        formatMeterVal(buf, sizeof(buf), audio.input(i)->peakLevel());
        rows_.push_back(buf);
    }
    for (int i = 0; i < audio.outputCount(); i++) {
        char buf[20];
        formatMeterVal(buf, sizeof(buf), audio.output(i)->peakLevel());
        rows_.push_back(buf);
    }

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    append(ListSection(a, "Input"));
    if (audio.inputCount() == 0) {
        ListEntry e; e.label = "No input devices";
        append(ListItemRow(a, e));
    } else {
        for (int i = 0; i < audio.inputCount(); i++) {
            ListEntry e;
            e.label = audio.input(i)->label();
            e.value = rows_[(size_t)i].c_str();
            append(ListItemRow(a, e));
        }
    }

    append(ListSection(a, "Output"));
    if (audio.outputCount() == 0) {
        ListEntry e; e.label = "No output devices";
        append(ListItemRow(a, e));
    } else {
        for (int i = 0; i < audio.outputCount(); i++) {
            ListEntry e;
            e.label = audio.output(i)->label();
            e.value = rows_[(size_t)(audio.inputCount() + i)].c_str();
            append(ListItemRow(a, e));
        }
        ListEntry beep; beep.label = "Test Beep 440Hz"; beep.chevron = true;
        beep.onPress = onTestBeep; beep.user = this;
        append(ListItemRow(a, beep));
    }

    return View(a, root, { TitleBar(a, "Sounds"), list });
}

} // namespace nema
