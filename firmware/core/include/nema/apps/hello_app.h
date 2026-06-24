#pragma once
#include "nema/app/component_app.h"
#include "nema/ui/node.h"

namespace nema {

// HelloApp — Plan 60 demo app showing the new widget system.
// Minimal example: TitleBar + SmartLabel + ListItem + Toggle.
class HelloApp : public ComponentApp {
public:
    const char* id()   const override { return "com.palanu.hello"; }
    const char* name() const override { return "Hello"; }

    void flipToggle() { toggleOn_ = !toggleOn_; }

protected:
    aether::ui::UiNode* build(aether::ui::NodeArena& arena, AppContext& ctx) override;

private:
    bool                    toggleOn_ = false;
    aether::ui::ScrollState scroll_;
};

} // namespace nema
