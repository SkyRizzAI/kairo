#include "kairo/screens/app_list_screen.h"
#include "kairo/runtime.h"
#include "kairo/plugin/plugin_manager.h"
#include "kairo/plugin/plugin.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/view_dispatcher.h"
#include <cstdio>

namespace kairo {

AppListScreen::AppListScreen(Runtime& rt) : rt_(rt) {}

void AppListScreen::buildList() {
    apps_.clear();
    for (auto* p : rt_.plugins().plugins())
        apps_.push_back({p->name(), p->id()});
    if (apps_.empty()) apps_.push_back({"No apps", ""});
    if (cursor_ >= (int)apps_.size()) cursor_ = (int)apps_.size() - 1;
}

void AppListScreen::enter() {
    buildList();
    rt_.view().requestRedraw();
}

void AppListScreen::update(Key key) {
    int sz = (int)apps_.size();
    switch (key) {
        case Key::Up:
            if (cursor_ > 0) { cursor_--; if (cursor_ < scroll_) scroll_--; }
            break;
        case Key::Down:
            if (cursor_ < sz - 1) { cursor_++; if (cursor_ >= scroll_ + VISIBLE_ROWS) scroll_++; }
            break;
        case Key::Select:
            if (!apps_[cursor_].id.empty())
                rt_.plugins().selectPlugin(apps_[cursor_].id.c_str());
            return;
        case Key::Cancel:
            rt_.view().pop();
            return;
        default: break;
    }
    rt_.view().requestRedraw();
}

void AppListScreen::drawList(Canvas& c) {
    uint16_t y = ui::CONTENT_Y + 2;
    for (int i = scroll_; i < (int)apps_.size() && i < scroll_ + VISIBLE_ROWS; i++) {
        bool sel = (i == cursor_);
        uint16_t row_y = y + (uint16_t)((i - scroll_) * ui::CHAR_H);
        char line[48];
        std::snprintf(line, sizeof(line), "> %s", apps_[i].name.c_str());
        if (sel) {
            uint16_t hw = c.textWidth(line) + 6;
            c.invertRect(2, row_y - 1, hw, ui::CHAR_H + 1);
        } else {
            std::snprintf(line, sizeof(line), "  %s", apps_[i].name.c_str());
        }
        c.drawText(5, row_y, line, !sel);
    }
}

void AppListScreen::draw(Canvas& c) {
    // Status bar drawn by runtime (Normal mode)
    drawList(c);
}

} // namespace kairo
