#pragma once
#include <functional>
#include <string>
#include <vector>

namespace nema {

class Runtime;

// CliService — a Flipper-style command interpreter (NOT a Unix shell: no kernel,
// no processes, no real filesystem). It is just a registry of named commands;
// each handler reads device state and streams text output back. Transport-
// agnostic: RemoteService routes the KLP CLI channel here, but the service itself
// only knows "a line in → lines out".
//
// Built to extend. add() is the single entry point, and is shared by:
//   - core built-ins (registerCoreCliCommands)
//   - platform specializations (e.g. ESP32 replaces `ram` with a live-heap read)
//   - future sources — a working-dir/file context once the FS lands, or `.kapp`
//     apps that expose their own commands ("bin"). None of those need to touch
//     the core; they just register more commands.
class CliService {
public:
    using Out     = std::function<void(const std::string&)>;  // emit one output line
    using Handler = std::function<void(const std::vector<std::string>& args, const Out& out)>;

    struct Command {
        std::string name;
        std::string help;
        Handler     handler;
    };

    // Register a command, or REPLACE the existing one with the same name. Replace
    // semantics let a platform/app specialize a built-in without removing it.
    void add(std::string name, std::string help, Handler handler);

    // Parse `line` into argv (whitespace-split) and dispatch to argv[0]. Output is
    // streamed via `out`, one call per logical line. Empty line → no-op; unknown
    // command → an error line. Handlers must be non-blocking (run on the link
    // thread); offload anything heavy to a TaskRunner.
    void execute(const std::string& line, const Out& out);

    const std::vector<Command>& commands() const { return cmds_; }

private:
    std::vector<Command> cmds_;
};

// Register the built-in commands: help, ram, hwinfo, caps, power, wlan/network,
// ble/bluetooth. They read everything through `rt` (introspection + container-
// resolved drivers), so the same set works on every platform; a driver that
// isn't present (e.g. BLE on the simulator) reports "not available".
void registerCoreCliCommands(CliService& cli, Runtime& rt);

} // namespace nema
