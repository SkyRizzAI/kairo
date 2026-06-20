#pragma once
#include "nema/ui/component_screen.h"

namespace nema {

class Runtime;

class DeveloperScreen : public ComponentScreen {
public:
    explicit DeveloperScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    void stopAether();
    void rebootBootloader();

    static void onStopAether(void* u);
    static void onRebootBootloader(void* u);

    aether::ui::ScrollState scroll_;
};

} // namespace nema
