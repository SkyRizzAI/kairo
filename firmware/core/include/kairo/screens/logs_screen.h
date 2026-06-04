#pragma once
#include "kairo/ui/screen.h"

namespace kairo {

class Runtime;

class LogsScreen : public IScreen {
public:
    explicit LogsScreen(Runtime& rt);
    void enter()         override;
    void update(Key key) override;
    void draw(Canvas& c) override;

private:
    Runtime& rt_;
};

} // namespace kairo
