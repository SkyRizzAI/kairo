// Plan 60 — LogsScreen: dense log view with level tags + auto-scroll.
#include "nema/screens/logs_screen.h"
#include "nema/runtime.h"
#include "nema/ui/style_tokens.h"
#include "nema/log/log_entry.h"
#include "nema/clock.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

LogsScreen::LogsScreen(Runtime& rt) : ComponentScreen(rt, 256) {}

void LogsScreen::onResume() {
    scroll_.scrollMain = 0x7FFF;   // renderer clamps to bottom → show newest
    ComponentScreen::onResume();
}

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

UiNode* LogsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();

    uint64_t ms = rt.clock().millis();
    uint32_t s = (uint32_t)(ms / 1000) % 60, m = (uint32_t)(ms / 60000) % 60,
             h = (uint32_t)(ms / 3600000);
    if (h > 0) std::snprintf(header_, sizeof(header_), "up %uh%um%us  %u logs",
                             (unsigned)h, (unsigned)m, (unsigned)s,
                             (unsigned)rt.logCount());
    else       std::snprintf(header_, sizeof(header_), "up %um%us  %u logs",
                             (unsigned)m, (unsigned)s, (unsigned)rt.logCount());

    rt.logForEach(&LogsScreen::collect, this);
    if (rows_.empty()) rows_.emplace_back("(no log entries)");

    uint8_t pad = aether::theme().space.sm;
    uint8_t gap = aether::theme().space.xs;

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = pad; root.gap = gap;
    root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col; sv.gap = 1;

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    for (auto& r : rows_) {
        UiNode* t = Text(a, r.c_str(), TextRole::Mono);
        if (!t) break;
        if (!prev) list->firstChild = t; else prev->nextSibling = t;
        prev = t;
    }

    return View(a, root, {
        TitleBar(a, "LOGS"),
        Text(a, header_, TextRole::Caption),
        list,
    });
}

} // namespace nema
