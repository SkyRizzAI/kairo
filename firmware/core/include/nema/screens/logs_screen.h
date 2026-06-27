#pragma once
#include "nema/ui/component_screen.h"
#include <memory>
#include <vector>
#include <string>

namespace nema {

class Runtime;

// Logs — live scrollable log view (Plan 60). A standard list-style toolbar at the
// top (a "Logs (<n>)" ListSection, a "Follow newest" SwitchRow and a "Clear logs"
// action) sits above a scrollable body of the most recent log entries, each
// prefixed with a level tag ([T]/[D]/[I]/[W]/[E]/[F]).
//
// Live update is COUNT-driven: tick() only marks the tree dirty when the log ring
// buffer's entry count changes, so an idle screen doesn't rebuild every frame.
// "Follow newest" keeps the body pinned to the bottom as new lines arrive; turn it
// off to hold a scroll position while reading back. "Clear logs" is gated behind a
// danger ConfirmModal.
class LogsScreen : public ComponentScreen {
public:
    explicit LogsScreen(Runtime& rt);
    void        onResume() override;
    void        tick(uint64_t nowMs) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState  scroll_;        // log body scroll position
    aether::ui::ScrollState  toolbarScroll_; // toolbar (never actually scrolls)
    std::vector<std::string> rows_;
    char                     header_[48]   = {};
    bool                     followNewest_ = true;
    size_t                   lastLogCount_ = (size_t)-1;  // force first rebuild
    int                      filterIdx_    = 0;           // 0=All, then Info+/Warn+/Error+

    std::unique_ptr<ComponentScreen> clearModal_;

    static void collect(void* ctx, const struct LogEntry& e);
    static void onToggleFollow(void* ctx);
    static void onAdjustLevel(void* ctx, int dir);
    static void onClearPressed(void* ctx);
    static void onClearConfirmed(void* ctx);
};

} // namespace nema
