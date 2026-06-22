#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

// Logs — dense scrollable log view (Plan 60). Header line (uptime/log count) +
// a ScrollView of the most recent log entries, each prefixed with a level tag
// ([T]/[D]/[I]/[W]/[E]/[F]). Auto-scrolls to newest on enter; uptime updates
// every second via tick().
class LogsScreen : public ComponentScreen {
public:
    explicit LogsScreen(Runtime& rt);
    void        onResume() override;
    void        tick(uint64_t nowMs) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState  scroll_;
    std::vector<std::string> rows_;
    char                     header_[64] = {};
    uint32_t                 lastSec_    = 0xFFFFFFFFu;

    static void collect(void* ctx, const struct LogEntry& e);
};

} // namespace nema
