#pragma once
#include "kairo/ui/screen.h"

namespace kairo {
class Runtime;

class CameraSettingsScreen : public IScreen {
public:
    explicit CameraSettingsScreen(Runtime& rt);

    void enter()         override;
    void update(Key key) override;
    void draw(Canvas& c) override;

private:
    Runtime& rt_;
    int      cursor_ = 0;
};

} // namespace kairo
