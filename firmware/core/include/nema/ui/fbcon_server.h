#pragma once
#include "nema/ui/display_server.h"
#include "nema/services/cli_service.h"
#include <string>
#include <vector>

namespace nema {

class Canvas;
class Runtime;

// FbconServer — Linux-style text console backend (Plan 43). Shows a boot banner
// and an interactive prompt with a compact Flipper-Zero-style virtual keyboard.
// Back toggles the keyboard; Prev/Next navigate keys; Activate types or submits.
class FbconServer : public IDisplayServer {
public:
    explicit FbconServer(Runtime& rt);
    const char* name() const override { return "fbcon"; }
    void renderFrame(Canvas& c, ViewDispatcher& views, const StatusBarData& status) override;
    bool onAction(input::Action action) override;

private:
    void executeInput();
    void histPrev();
    void histNext();
    void drawKeyboard(Canvas& c) const;

    Runtime&    rt_;
    CliSession  session_;
    std::string inputBuf_;
    int         histIdx_ = -1;
    std::vector<std::string> outputLines_;

    // Virtual keyboard
    bool kbdOpen_ = false;
    int  kbdRow_  = 0;
    int  kbdCol_  = 0;
};

} // namespace nema
