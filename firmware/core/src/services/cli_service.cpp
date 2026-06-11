#include "kairo/services/cli_service.h"
#include "kairo/services/profile_service.h"
#include "kairo/runtime.h"
#include "kairo/board.h"
#include "kairo/system/system_info.h"
#include "kairo/system/hardware_registry.h"
#include "kairo/system/capability_registry.h"
#include "kairo/service/service_container.h"
#include "kairo/hal/wifi.h"
#include "kairo/hal/bluetooth.h"
#include "kairo/hal/filesystem.h"
#include <cctype>

namespace kairo {

void CliService::add(std::string name, std::string help, Handler handler) {
    for (auto& c : cmds_) {
        if (c.name == name) {                 // replace (specialize) an existing command
            c.help = std::move(help);
            c.handler = std::move(handler);
            return;
        }
    }
    cmds_.push_back({std::move(name), std::move(help), std::move(handler)});
}

void CliService::execute(const std::string& line, const Out& out) {
    std::vector<std::string> argv;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace((unsigned char)line[i])) i++;
        size_t start = i;
        while (i < line.size() && !std::isspace((unsigned char)line[i])) i++;
        if (i > start) argv.push_back(line.substr(start, i - start));
    }
    if (argv.empty()) return;

    for (auto& c : cmds_) {
        if (c.name == argv[0]) {
            c.handler({argv.begin() + 1, argv.end()}, out);
            return;
        }
    }
    out("unknown command: " + argv[0] + " (try 'help')");
}

namespace {

const char* driverKindName(DriverKind k) {
    switch (k) {
        case DriverKind::Battery:   return "battery";
        case DriverKind::Wifi:      return "wifi";
        case DriverKind::Bluetooth: return "bluetooth";
        case DriverKind::Display:   return "display";
        case DriverKind::Storage:   return "storage";
        default:                    return "other";
    }
}

} // namespace

void registerCoreCliCommands(CliService& cli, Runtime& rt) {
    Runtime* r = &rt;

    cli.add("help", "list commands (help <cmd> for detail)",
        [&cli](const std::vector<std::string>& args, const CliService::Out& out) {
            if (!args.empty()) {
                for (auto& c : cli.commands())
                    if (c.name == args[0]) { out(c.name + " — " + c.help); return; }
                out("no such command: " + args[0]);
                return;
            }
            out("Commands:");
            for (auto& c : cli.commands()) out("  " + c.name + "  —  " + c.help);
        });

    cli.add("hwinfo", "board, chip and device summary",
        [r](const std::vector<std::string>&, const CliService::Out& out) {
            const SystemInfo& si = r->info();
            out(std::string("board:    ") + r->board().name());
            out("platform: " + si.platformName);
            out("fw:       " + si.firmwareVersion);
            out("build:    " + si.buildVersion);
            if (si.cpuMhz)  out("cpu:      " + std::to_string(si.cpuMhz) + " MHz");
            if (si.ramKb)   out("ram:      " + std::to_string(si.ramKb) + " KB");
            if (si.psramKb) out("psram:    " + std::to_string(si.psramKb) + " KB");
            if (si.flashKb) out("flash:    " + std::to_string(si.flashKb) + " KB");
            out("devices:");
            for (auto& e : r->hardware().list())
                out("  " + e.id + " [" + driverKindName(e.kind) + "] " + e.detail);
        });

    cli.add("ram", "memory totals",
        [r](const std::vector<std::string>&, const CliService::Out& out) {
            const SystemInfo& si = r->info();
            out("ram:   " + std::to_string(si.ramKb) + " KB");
            out("psram: " + std::to_string(si.psramKb) + " KB");
        });

    cli.add("caps", "list runtime capabilities",
        [r](const std::vector<std::string>&, const CliService::Out& out) {
            for (auto& c : r->capabilities().list()) out(c);
        });

    cli.add("power", "power control: power restart|shutdown",
        [r](const std::vector<std::string>& args, const CliService::Out& out) {
            if (args.empty()) { out("usage: power restart|shutdown"); return; }
            if (args[0] == "restart")  { out("restarting…");   r->requestRestart(); }
            else if (args[0] == "shutdown") { out("shutting down…"); r->requestShutdown(); }
            else out("unknown: " + args[0] + " (restart|shutdown)");
        });

    auto wlan = [r](const std::vector<std::string>&, const CliService::Out& out) {
        auto* wifi = r->container().resolve<IWifiDriver>();
        if (!wifi) { out("wifi: not available"); return; }
        out(std::string("status: ") + (wifi->isConnected() ? "connected" : "disconnected"));
        const char* s = wifi->ssid();
        out(std::string("ssid:   ") + (s && *s ? s : "—"));
        const char* ip = wifi->ip();
        out(std::string("ip:     ") + (ip && *ip ? ip : "—"));
        out("scanned: " + std::to_string(wifi->scanResults().size()) + " networks");
    };
    cli.add("wlan", "WiFi status (ssid, ip, connection)", wlan);
    cli.add("network", "alias of wlan", wlan);

    auto ble = [r](const std::vector<std::string>&, const CliService::Out& out) {
        auto* ctl = r->container().resolve<IBluetoothController>();
        if (!ctl) { out("bluetooth: not available"); return; }
        out(std::string("enabled: ") + (ctl->isEnabled() ? "yes" : "no"));
        const char* addr = ctl->address();
        out(std::string("address: ") + (addr && *addr ? addr : "—"));
        const char* name = ctl->deviceName();
        out(std::string("name:    ") + (name && *name ? name : "—"));
        if (auto* a = r->container().resolve<IBleAdapter>()) {
            out(std::string("adv:     ") + (a->isAdvertising() ? "yes" : "no"));
            out(std::string("conn:    ") + (a->isConnected() ? "yes" : "no"));
            out("bonded:  " + std::to_string(a->bondedCount()));
        }
    };
    cli.add("ble", "Bluetooth/BLE radio status", ble);
    cli.add("bluetooth", "alias of ble", ble);

    cli.add("whoami", "show owner user and device name",
        [r](const std::vector<std::string>&, const CliService::Out& out) {
            auto* p = r->container().resolve<ProfileService>();
            if (!p) { out("profile: not available"); return; }
            out("user:   " + p->userName());
            out("device: " + p->deviceName());
        });

    cli.add("profile", "view/edit owner profile (set user|device, passwd, verify)",
        [r](const std::vector<std::string>& args, const CliService::Out& out) {
            auto* p = r->container().resolve<ProfileService>();
            if (!p) { out("profile: not available"); return; }
            if (args.empty()) {
                out("user:     " + p->userName());
                out("device:   " + p->deviceName());
                out(std::string("password: ") + (p->hasPassword() ? "set" : "not set"));
                return;
            }
            const std::string& sub = args[0];
            if (sub == "set") {
                if (args.size() < 3) { out("usage: profile set user|device <value>"); return; }
                if      (args[1] == "user")   { p->setUserName(args[2]);   out("user set: "   + args[2]); }
                else if (args[1] == "device") { p->setDeviceName(args[2]); out("device set: " + args[2]); }
                else out("usage: profile set user|device <value>");
            } else if (sub == "passwd") {
                if (args.size() < 2) { out("usage: profile passwd <new> | --clear"); return; }
                if (args[1] == "--clear") { p->clearPassword(); out("password cleared"); }
                else                      { p->setPassword(args[1]); out("password set"); }
            } else if (sub == "verify") {
                if (args.size() < 2) { out("usage: profile verify <input>"); return; }
                out(p->verifyPassword(args[1]) ? "ok" : "no");
            } else {
                out("usage: profile [set user|device <val>] [passwd <new>|--clear] [verify <input>]");
            }
        });

    cli.add("fs", "filesystem: fs ls|cat|rm|mkdir [path]",
        [r](const std::vector<std::string>& args, const CliService::Out& out) {
            auto* fs = r->container().resolve<IFileSystem>();
            if (!fs) { out("fs: not available"); return; }
            std::string sub = args.empty() ? "ls" : args[0];
            std::string path = args.size() > 1 ? args[1] : "/";
            if (sub == "ls") {
                std::vector<FsEntry> es;
                if (!fs->list(path, es)) { out("no such directory: " + path); return; }
                for (auto& e : es)
                    out((e.isDir ? "d " : "- ") + e.name +
                        (e.isDir ? "" : "  " + std::to_string(e.size) + "B"));
            } else if (sub == "cat") {
                if (args.size() < 2) { out("usage: fs cat <path>"); return; }
                std::vector<uint8_t> data;
                if (!fs->read(path, data)) { out("no such file: " + path); return; }
                out(std::string((const char*)data.data(), data.size()));
            } else if (sub == "rm") {
                if (args.size() < 2) { out("usage: fs rm <path>"); return; }
                out(fs->remove(path) ? "removed " + path : "failed: " + path);
            } else if (sub == "mkdir") {
                if (args.size() < 2) { out("usage: fs mkdir <path>"); return; }
                out(fs->mkdir(path) ? "created " + path : "failed: " + path);
            } else {
                out("usage: fs ls|cat|rm|mkdir [path]");
            }
        });
}

} // namespace kairo
