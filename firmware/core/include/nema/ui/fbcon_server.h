#pragma once
#include "nema/ui/display_server.h"
#include "nema/services/cli_service.h"
#include <string>
#include <vector>

namespace nema {

class Runtime;

// FbconServer — Linux-style text console backend (Plan 43). Shows a boot banner
// and an interactive prompt; physical buttons navigate history and submit lines
// to the CliService. Boot default pre-fills "display start aether" so pressing
// OK immediately starts the UI.
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

    Runtime&    rt_;
    CliSession  session_;               // local TTY session: owns history
    std::string inputBuf_;              // current line being composed
    int         histIdx_ = -1;          // -1 = not in history browse mode
    std::vector<std::string> outputLines_;  // output lines shown above the prompt
};

} // namespace nema
