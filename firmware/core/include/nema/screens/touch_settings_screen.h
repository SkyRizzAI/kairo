#pragma once
#include "nema/ui/component_screen.h"

namespace nema {

class Runtime;

// Settings → Touch. Placeholder until touch configuration is implemented.
class TouchSettingsScreen : public ComponentScreen {
public:
    explicit TouchSettingsScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState scroll_;
};

} // namespace nema
