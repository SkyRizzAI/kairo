#pragma once
#include "nema/ui/component_screen.h"
#include <memory>

namespace nema {

class Runtime;

class DeveloperScreen : public ComponentScreen {
public:
    explicit DeveloperScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    void onConfirmStopAether();
    void onConfirmReboot();

    aether::ui::ScrollState    scroll_;
    std::unique_ptr<ComponentScreen> stopModal_;
    std::unique_ptr<ComponentScreen> rebootModal_;
};

} // namespace nema
