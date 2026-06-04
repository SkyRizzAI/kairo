#pragma once
#include "kairo/ui/screen.h"
#include <vector>
#include <string>
#include <cstdint>

namespace kairo {

class Runtime;

class AppListScreen : public IScreen {
public:
    explicit AppListScreen(Runtime& rt);
    void enter()              override;
    void update(Key key)      override;
    void draw(Canvas& c)      override;
    // tick() not needed — status bar managed by runtime

private:
    Runtime& rt_;
    int      cursor_ = 0;
    int      scroll_ = 0;

    struct AppEntry { std::string name; std::string id; };
    std::vector<AppEntry> apps_;

    static constexpr int VISIBLE_ROWS = 8;

    void buildList();
    void drawList(Canvas& c);
};

} // namespace kairo
