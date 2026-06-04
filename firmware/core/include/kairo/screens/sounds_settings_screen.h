#pragma once
#include "kairo/ui/screen.h"

namespace kairo {
class Runtime;

class SoundsSettingsScreen : public IScreen {
public:
    explicit SoundsSettingsScreen(Runtime& rt);

    void enter()              override;
    void tick(uint64_t nowMs) override;
    void update(Key key)      override;
    void draw(Canvas& c)      override;

private:
    // Draw one device row: label + "0 [bar] CUR /100"
    void drawDeviceRow(Canvas& c, uint16_t y, bool sel,
                       const char* label, float level) const;

    Runtime& rt_;
    int      cursor_ = 0;
};

} // namespace kairo
