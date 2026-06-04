#pragma once
#include "kairo/ui/screen.h"
#include "kairo/apps/touch_test_app.h"
#include <memory>
#include <vector>

namespace kairo {

class Runtime;
class AppHost;

// Settings → Touch. Submenu for touch-related features. For now just launches
// the Touch Test diagnostic; room for calibration / sensitivity later.
class TouchSettingsScreen : public IScreen {
public:
    explicit TouchSettingsScreen(Runtime& rt);
    ~TouchSettingsScreen() override;   // out-of-line: AppHost incomplete here
    void enter()         override;
    void update(Key key) override;
    void draw(Canvas& c) override;

private:
    Runtime&                 rt_;
    TouchTestApp             touchApp_;
    std::unique_ptr<AppHost> touchHost_;
    int                      cursor_ = 0;

    struct Item { const char* label; };
    std::vector<Item> items_;

    void handleSelect();
};

} // namespace kairo
