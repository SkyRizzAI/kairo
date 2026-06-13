#pragma once
#include "nema/ui/display_server.h"

namespace nema {

class Runtime;

// FbconServer — a minimal text "console" display backend (Plan 43), analogous
// to Linux fbcon / AkiraOS shell_display. It ignores the app view tree and
// instead paints a system console (board identity, uptime, capabilities) to the
// panel. It is the always-available fallback surface and the second backend
// that proves the renderer is swappable at runtime (`display switch fbcon`).
class FbconServer : public IDisplayServer {
public:
    explicit FbconServer(Runtime& rt) : rt_(rt) {}
    const char* name() const override { return "fbcon"; }
    void renderFrame(Canvas& c, ViewDispatcher& views, const StatusBarData& status) override;

private:
    Runtime& rt_;
};

} // namespace nema
