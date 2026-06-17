#include "nema/apps/clock_app.h"
#include "nema/ui/widgets.h"
#include <ctime>
#include <cstdio>

namespace nema {

using namespace ui;

static const char* DAYS[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char* MONTHS[] = {"Jan","Feb","Mar","Apr","May","Jun",
                               "Jul","Aug","Sep","Oct","Nov","Dec"};

void ClockApp::snapshot() {
    time_t t = std::time(nullptr);
    struct tm* tm = localtime(&t);
    if (!tm) return;
    lastSec_ = tm->tm_sec;
    std::snprintf(timeBuf_, sizeof(timeBuf_), "%02d:%02d:%02d",
                  tm->tm_hour, tm->tm_min, tm->tm_sec);
    std::snprintf(dateBuf_, sizeof(dateBuf_), "%s, %02d %s %d",
                  DAYS[tm->tm_wday], tm->tm_mday, MONTHS[tm->tm_mon], tm->tm_year + 1900);
}

bool ClockApp::onTick(AppContext&) {
    time_t t = std::time(nullptr);
    struct tm* tm = localtime(&t);
    if (!tm || tm->tm_sec == lastSec_) return false;   // skip repaint if unchanged
    snapshot();
    return true;
}

UiNode* ClockApp::build(NodeArena& a, AppContext&) {
    if (lastSec_ < 0) snapshot();

    Style root;
    root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 6;
    root.align = Align::Center; root.justify = Justify::Center;

    return View(a, root, {
        Text(a, timeBuf_, TextRole::Title),
        Text(a, dateBuf_, TextRole::Body),
    });
}

} // namespace nema
