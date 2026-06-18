#include "nema/ui/app_window.h"
#include "nema/ui/canvas.h"

namespace nema {

AppWindow::AppWindow(const char* appName)
    : appName_(appName) {}

void AppWindow::onResume() {
    if (auto* s = vd_.active()) s->onResume();
}

void AppWindow::onPause() {
    if (auto* s = vd_.active()) s->onPause();
}

void AppWindow::onStop() {
    // Stop all screens in the internal stack (bottom to top)
    // ViewDispatcher doesn't expose iteration, so we stop the active only.
    if (auto* s = vd_.active()) s->onStop();
}

bool AppWindow::onBackPressed() {
    // If the internal VD can go back, do it and return true (consumed).
    if (vd_.canGoBack()) {
        vd_.goBack();
        return true;
    }
    return false;   // let the system pop the AppWindow itself
}

void AppWindow::onAction(input::Action a) {
    if (auto* s = vd_.active()) s->onAction(a);
}

void AppWindow::onCode(input::Code c) {
    if (auto* s = vd_.active()) s->onCode(c);
}

void AppWindow::onPointer(const input::PointerEvent& e) {
    if (auto* s = vd_.active()) s->onPointer(e);
}

void AppWindow::draw(Canvas& c) {
    if (auto* s = vd_.active()) s->draw(c);
}

void AppWindow::tick(uint64_t nowMs) {
    if (auto* s = vd_.active()) s->tick(nowMs);
}

} // namespace nema
