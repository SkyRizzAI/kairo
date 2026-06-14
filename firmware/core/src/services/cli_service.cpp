#include "nema/services/cli_service.h"
#include "nema/services/profile_service.h"
#include "nema/runtime.h"
#include "nema/board.h"
#include "nema/system/system_info.h"
#include "nema/system/hardware_registry.h"
#include "nema/system/capability_registry.h"
#include "nema/service/service_container.h"
#include "nema/service.h"
#include "nema/app/app_host_manager.h"
#include "nema/hal/wifi.h"
#include "nema/hal/bluetooth.h"
#include "nema/hal/filesystem.h"
#include "nema/hal/ota.h"
#include <cctype>

namespace nema {

void CliSession::pushHistory(const std::string& line) {
    if (line.empty()) return;
    if (!history.empty() && history.back() == line) return;  // skip immediate dupes
    history.push_back(line);
    if (history.size() > 32) history.erase(history.begin());  // capped ring
}

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

void CliService::execute(const std::string& line, CliSession& session) {
    std::vector<std::string> argv;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace((unsigned char)line[i])) i++;
        size_t start = i;
        while (i < line.size() && !std::isspace((unsigned char)line[i])) i++;
        if (i > start) argv.push_back(line.substr(start, i - start));
    }
    if (argv.empty()) return;
    session.pushHistory(line);

    std::vector<std::string> args(argv.begin() + 1, argv.end());
    CliContext ctx{args, session.out, session};
    for (auto& c : cmds_) {
        if (c.name == argv[0]) {
            c.handler(ctx);
            return;
        }
    }
    session.out("unknown command: " + argv[0] + " (try 'help')");
}

void CliService::execute(const std::string& line, const Out& out) {
    CliSession tmp;            // throwaway: no persisted history/cwd
    tmp.out = out;
    execute(line, tmp);
}

CliSession& CliSessionManager::get(uint8_t sid) {
    for (auto& s : sessions_)
        if (s->id == sid) return *s;
    sessions_.push_back(std::make_unique<CliSession>());
    sessions_.back()->id = sid;
    return *sessions_.back();
}

void CliSessionManager::remove(uint8_t sid) {
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
        if ((*it)->id == sid) { sessions_.erase(it); return; }
}

void CliSessionManager::clear() { sessions_.clear(); }

namespace {

const char* serviceStateStr(ServiceState s) {
    switch (s) {
        case ServiceState::Created:  return "created";
        case ServiceState::Starting: return "starting";
        case ServiceState::Running:  return "running";
        case ServiceState::Stopping: return "stopping";
        case ServiceState::Stopped:  return "stopped";
        case ServiceState::Failed:   return "failed";
    }
    return "?";
}

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

// Resolve `p` against the session's cwd into a normalized absolute path,
// honouring "/", relative paths, "." and "..". (Plan 44 — stateful fs.)
std::string resolvePath(const std::string& cwd, const std::string& p) {
    std::string base = p.empty() ? cwd
                     : (p[0] == '/' ? p : (cwd == "/" ? "/" + p : cwd + "/" + p));
    std::vector<std::string> parts;
    std::string cur;
    base.push_back('/');                       // sentinel to flush the last segment
    for (char ch : base) {
        if (ch == '/') {
            if (cur == "..") { if (!parts.empty()) parts.pop_back(); }
            else if (cur != "." && !cur.empty()) parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    std::string out = "/";
    for (size_t i = 0; i < parts.size(); i++) {
        out += parts[i];
        if (i + 1 < parts.size()) out += "/";
    }
    return out;
}

} // namespace

void registerCoreCliCommands(CliService& cli, Runtime& rt) {
    Runtime* r = &rt;

    cli.add("help", "list commands (help <cmd> for detail)",
        [&cli](CliContext& c) {
            const auto& args = c.args; const auto& out = c.out;
            if (!args.empty()) {
                for (auto& cmd : cli.commands())
                    if (cmd.name == args[0]) { out(cmd.name + " — " + cmd.help); return; }
                out("no such command: " + args[0]);
                return;
            }
            out("Commands:");
            for (auto& cmd : cli.commands()) out("  " + cmd.name + "  —  " + cmd.help);
        });

    // ── shell built-ins (Plan 44): stateful, per-session ──
    cli.add("pwd", "print working directory",
        [](CliContext& c) { c.out(c.session.cwd); });

    cli.add("cd", "change working directory: cd <dir>",
        [r](CliContext& c) {
            std::string target = resolvePath(c.session.cwd, c.args.empty() ? "/" : c.args[0]);
            auto* fs = r->container().resolve<IFileSystem>();
            if (!fs) { c.out("fs: not available"); return; }
            std::vector<FsEntry> es;
            if (target != "/" && !fs->list(target, es)) { c.out("no such directory: " + target); return; }
            c.session.cwd = target;
        });

    cli.add("history", "show this session's command history",
        [](CliContext& c) {
            for (size_t i = 0; i < c.session.history.size(); i++)
                c.out(std::to_string(i + 1) + "  " + c.session.history[i]);
        });

    cli.add("sessions", "list active CLI sessions (id, cwd, history)",
        [r](CliContext& c) {
            for (auto& s : r->cliSessions().sessions()) {
                std::string mark = (s->id == c.session.id) ? " *" : "";
                c.out("#" + std::to_string(s->id) + mark +
                      "  cwd=" + s->cwd +
                      "  hist=" + std::to_string(s->history.size()));
            }
        });

    cli.add("ps", "process monitor: services, apps, sessions",
        [r](CliContext& c) {
            const auto& out = c.out;
            out("SERVICES:");
            for (auto* svc : r->container().services())
                out(std::string("  ") + svc->name() +
                    "  [" + serviceStateStr(r->serviceState(svc)) + "]");
            out("APPS:");
            bool any = false;
            if (r->appHost().hasForeground()) {
                out(std::string("  ") + r->appHost().foregroundName() + "  [foreground]");
                any = true;
            }
            if (r->appHost().hasPaused()) {
                out(std::string("  ") + r->appHost().pausedName() + "  [paused]");
                any = true;
            }
            if (!any) out("  (none)");
            out("SESSIONS:");
            for (auto& s : r->cliSessions().sessions())
                out("  #" + std::to_string(s->id) + "  cwd=" + s->cwd);
        });

    cli.add("hwinfo", "board, chip and device summary",
        [r](CliContext& c) {
            const auto& out = c.out;
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
        [r](CliContext& c) {
            const auto& out = c.out;
            const SystemInfo& si = r->info();
            out("ram:   " + std::to_string(si.ramKb) + " KB");
            out("psram: " + std::to_string(si.psramKb) + " KB");
        });

    cli.add("caps", "list runtime capabilities",
        [r](CliContext& c) {
            for (auto& cap : r->capabilities().list()) c.out(cap);
        });

    cli.add("display", "display server: display [list | start <backend>]",
        [r](CliContext& c) {
            const auto& args = c.args; const auto& out = c.out;
            if (args.empty()) {
                out(std::string("active: ") + r->displayServerName());
                out("backends:");
                for (auto* n : r->displayServerList()) out(std::string("  ") + n);
                out("usage: display start <backend>  (e.g. aether | fbcon)");
                return;
            }
            if (args[0] == "list") {
                for (auto* n : r->displayServerList()) out(n);
                return;
            }
            const std::string& target =
                (args[0] == "start" && args.size() > 1) ? args[1] : args[0];
            out(r->switchDisplayServer(target.c_str())
                    ? "switched to " + target
                    : "unknown backend: " + target);
        });

    cli.add("ota", "firmware update status (Plan 39)",
        [r](CliContext& c) {
            auto* ota = r->container().resolve<IOtaUpdater>();
            if (!ota || !ota->supported()) { c.out("ota: not supported on this platform"); return; }
            c.out(std::string("running slot: ") + ota->runningSlot());
            c.out("push a new image from Forge → \"Update firmware\" (PLP over USB/BLE)");
        });

    cli.add("power", "power control: power restart|shutdown",
        [r](CliContext& c) {
            const auto& args = c.args; const auto& out = c.out;
            if (args.empty()) { out("usage: power restart|shutdown"); return; }
            if (args[0] == "restart")  { out("restarting…");   r->requestRestart(); }
            else if (args[0] == "shutdown") { out("shutting down…"); r->requestShutdown(); }
            else out("unknown: " + args[0] + " (restart|shutdown)");
        });

    auto wlan = [r](CliContext& c) {
        const auto& out = c.out;
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

    auto ble = [r](CliContext& c) {
        const auto& out = c.out;
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
        [r](CliContext& c) {
            const auto& out = c.out;
            auto* p = r->container().resolve<ProfileService>();
            if (!p) { out("profile: not available"); return; }
            out("user:   " + p->userName());
            out("device: " + p->deviceName());
        });

    cli.add("profile", "view/edit owner profile (set user|device, passwd, verify)",
        [r](CliContext& c) {
            const auto& args = c.args; const auto& out = c.out;
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

    cli.add("fs", "filesystem: fs ls|cat|rm|mkdir [path] (relative to cwd)",
        [r](CliContext& c) {
            const auto& args = c.args; const auto& out = c.out;
            auto* fs = r->container().resolve<IFileSystem>();
            if (!fs) { out("fs: not available"); return; }
            std::string sub = args.empty() ? "ls" : args[0];
            // ls defaults to cwd; the rest resolve their arg relative to cwd.
            std::string path = (args.size() > 1) ? resolvePath(c.session.cwd, args[1])
                             : (sub == "ls" ? c.session.cwd : "/");
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

} // namespace nema
