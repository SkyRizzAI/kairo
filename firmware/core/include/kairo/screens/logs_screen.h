#pragma once
#include "kairo/ui/component_screen.h"
#include <vector>
#include <string>

namespace kairo {

class Runtime;

// Logs — component-migrated (Plan 30). Header + scrollable stat/log rows.
class LogsScreen : public ComponentScreen {
public:
    explicit LogsScreen(Runtime& rt);
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    ui::ScrollState          scroll_;
    std::vector<std::string> rows_;
};

} // namespace kairo
