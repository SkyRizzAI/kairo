// Plan 60 — LogsScreen: live log view with a standard list toolbar (Follow /
// Clear) above a dense, auto-scrolling Mono body.
#include "nema/screens/logs_screen.h"
#include "nema/screens/confirm_modal.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/log/log_entry.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

LogsScreen::LogsScreen(Runtime& rt) : ComponentScreen(rt, 256) {
    auto* m = new ConfirmModal(rt);
    m->setup("Clear logs?", "Remove all log\nentries?", "Clear",
             &LogsScreen::onClearConfirmed, this, /*danger=*/true);
    clearModal_.reset(m);
}

void LogsScreen::onResume() {
    if (followNewest_) scroll_.scrollMain = 0x7FFF;  // renderer clamps to bottom
    lastLogCount_ = (size_t)-1;                       // re-sync on first rebuild
    ComponentScreen::onResume();                      // sets dirty_ + redraw
}

void LogsScreen::tick(uint64_t nowMs) {
    ComponentScreen::tick(nowMs);   // momentum / marquee animation
    // Live update is COUNT-driven: only rebuild the tree when new entries arrive
    // (or the ring buffer was cleared). An idle screen never rebuilds.
    size_t count = rt_.logCount();
    if (count != lastLogCount_) {
        lastLogCount_ = count;
        markDirty();                // dirty_ = true + requestRedraw → rebuild
    }
}

// Toolbar min-level filter cycle: show entries at or above the chosen level.
static const LogLevel kFilterLevels[] = { LogLevel::Trace, LogLevel::Info, LogLevel::Warn, LogLevel::Error };
static const char*    kFilterNames[]  = { "All", "Info+", "Warn+", "Error" };
static constexpr int  kFilterCount    = 4;

static char levelTag(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return 'T';
        case LogLevel::Debug: return 'D';
        case LogLevel::Info:  return 'I';
        case LogLevel::Warn:  return 'W';
        case LogLevel::Error: return 'E';
        case LogLevel::Fatal: return 'F';
        default:              return '?';
    }
}

void LogsScreen::collect(void* ctx, const LogEntry& e) {
    auto* self = static_cast<LogsScreen*>(ctx);
    if ((uint8_t)e.level < (uint8_t)kFilterLevels[self->filterIdx_]) return;  // below filter
    constexpr size_t MAX_ROWS = 80;
    char buf[80];
    std::snprintf(buf, sizeof(buf), "[%c] %s: %s",
                  levelTag(e.level),
                  e.component ? e.component : "?",
                  e.message.c_str());
    self->rows_.emplace_back(buf);
    if (self->rows_.size() > MAX_ROWS)
        self->rows_.erase(self->rows_.begin());
}

void LogsScreen::onToggleFollow(void* ctx) {
    auto* self = static_cast<LogsScreen*>(ctx);
    self->followNewest_ = !self->followNewest_;
    if (self->followNewest_) self->scroll_.scrollMain = 0x7FFF;  // jump to newest
    self->markDirty();
}

void LogsScreen::onAdjustLevel(void* ctx, int dir) {
    auto* self = static_cast<LogsScreen*>(ctx);
    self->filterIdx_ = (self->filterIdx_ + dir + kFilterCount) % kFilterCount;
    self->markDirty();
}

void LogsScreen::onClearPressed(void* ctx) {
    auto* self = static_cast<LogsScreen*>(ctx);
    self->rt_.view().push(*self->clearModal_);
}

void LogsScreen::onClearConfirmed(void* ctx) {
    auto* self = static_cast<LogsScreen*>(ctx);
    self->rt_.logClear();
    self->lastLogCount_ = 0;
    self->scroll_.scrollMain = 0x7FFF;
    self->markDirty();
    self->rt_.view().goBack();      // pop the confirm modal → back to logs
}

UiNode* LogsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    rt.logForEach(&LogsScreen::collect, this);
    size_t count = rt.logCount();
    lastLogCount_ = count;          // keep tick()'s comparison in sync

    std::snprintf(header_, sizeof(header_), "Logs (%u)", (unsigned)count);

    // ── Fixed toolbar (stays put while the body follows newest) ──────────────
    // Same 2px inset / 2px gap as ListContainer, but NOT a ScrollView so the
    // controls never scroll out of reach.
    Style tb; tb.dir = FlexDir::Col; tb.align = Align::Stretch;
    tb.padding = 2; tb.gap = 2;
    UiNode* toolbar = View(a, tb, {});
    UiNode* tprev = nullptr;
    auto addTool = [&](UiNode* n) {
        if (!n) return;
        if (!tprev) toolbar->firstChild = n; else tprev->nextSibling = n;
        tprev = n;
    };
    addTool(ListSection(a, header_));
    addTool(SwitchRow(a, "Follow newest", followNewest_,
                      &LogsScreen::onToggleFollow, this));
    {
        ListInput lvl; lvl.label = "Level"; lvl.value = kFilterNames[filterIdx_];
        lvl.onAdjust = &LogsScreen::onAdjustLevel; lvl.user = this;
        addTool(ListInputRow(a, lvl));
    }
    if (count > 0) {
        ListEntry clear; clear.label = "Clear logs";
        clear.onPress = &LogsScreen::onClearPressed; clear.user = this;
        addTool(ListItemRow(a, clear));
    }

    // ── Scrollable log body (Mono lines; non-focusable → cheap per line) ─────
    Style sv; sv.dir = FlexDir::Col; sv.align = Align::Stretch;
    sv.flexGrow = 1; sv.gap = 1; sv.padding = 2;
    UiNode* body = ScrollView(a, scroll_, sv, {});
    if (rows_.empty()) {
        body->firstChild = Text(a, "No logs yet", TextRole::Mono);
    } else {
        UiNode* prev = nullptr;
        for (auto& r : rows_) {
            UiNode* t = Text(a, r.c_str(), TextRole::Mono);
            if (!t) break;
            if (!prev) body->firstChild = t; else prev->nextSibling = t;
            prev = t;
        }
    }

    // Follow-newest: pin the body to the bottom every rebuild (layout clamps the
    // sentinel offset to maxScroll). When off, leave the user's scroll untouched.
    if (followNewest_) scroll_.scrollMain = 0x7FFF;

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;
    return View(a, root, { toolbar, body });
}

} // namespace nema
