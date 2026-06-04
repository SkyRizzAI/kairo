#pragma once
#include "kairo/ui/screen.h"
#include "kairo/input/input_action.h"

namespace kairo {

class Runtime;

// Read-only introspection of the input system: board name, button count,
// Action → hint mapping, and gesture timing parameters.
// Accessed via Settings → Controls.
class ControlsScreen : public IScreen {
public:
    explicit ControlsScreen(Runtime& rt);

    void enter()                        override;
    void onAction(input::Action a)      override;
    void draw(Canvas& c)               override;

private:
    Runtime& rt_;
    int      scroll_ = 0;

    static constexpr int VISIBLE_ROWS = 4;
};

} // namespace kairo
