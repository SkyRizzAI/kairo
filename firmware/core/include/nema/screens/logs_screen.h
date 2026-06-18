#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

// Logs — dense scrollable log view (Plan 60). Header line (uptime/app count) +
// a ScrollView of the most recent log entries, each prefixed with a level tag
// ([T]/[D]/[I]/[W]/[E]/[F]). Auto-scrolls to the newest entry on enter.
class LogsScreen : public ComponentScreen {
public:
    explicit LogsScreen(Runtime& rt);
    void        onResume() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    ui::ScrollState          scroll_;
    std::vector<std::string> rows_;
    char                     header_[64] = "";  // stable for the Text node

    // Callback target for Runtime::logForEach — appends a formatted row.
    static void collect(void* ctx, const struct LogEntry& e);
};

} // namespace nema
