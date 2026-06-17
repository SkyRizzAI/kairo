#pragma once
#include "nema/ui/component_screen.h"

namespace nema {

class Runtime;

class DeveloperScreen : public ComponentScreen {
public:
    explicit DeveloperScreen(Runtime& rt);
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    void stopAether();

    static void onStopAether(void* u);

    ui::ScrollState scroll_;
};

} // namespace nema
