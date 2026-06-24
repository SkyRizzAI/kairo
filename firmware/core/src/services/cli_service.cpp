#include "nema/services/cli_service.h"
#include "nema/services/profile_service.h"
#include "nema/runtime.h"
#include "nema/board.h"
#include "nema/app/app.h"
#include "nema/app/app_registry.h"
#include "nema/proc/process_host.h"
#include "nema/proc/pipe.h"
#include "nema/app/papp_installer.h"
#include "nema/apps/js_app_store.h"
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
#include "nema/ui/view_dispatcher.h"
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

    // ── Auto-launch: check if argv[0] matches an installed app (Plan 57) ──
    // Scans AppRegistry first, then PATH directories for .papp bundles.
    if (!rt_) { session.out("unknown command: " + argv[0] + " (try 'help')"); return; }
    auto& rt = *rt_;

    // ── Helper: recursively scan a PATH directory for matching .papp ──
    auto endsWith = [](const std::string& s, const std::string& suffix) {
        return s.size() >= suffix.size() &&
               s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    std::function<bool(const std::string&)> findAppInDir;
    IFileSystem* fs = rt.container().resolve<IFileSystem>();
    findAppInDir = [&](const std::string& dir) -> bool {
        if (!fs) return false;
        std::vector<FsEntry> es;
        if (!fs->list(dir, es)) return false;
        for (auto& e : es) {
            std::string name(e.name);
            if (e.isDir) {
                // .papp folder: match folder name exactly
                if (name == argv[0] + ".papp") return true;
                // Recurse into non-.papp subdirs too
                if (!endsWith(name, ".papp")) {
                    if (findAppInDir(dir + "/" + name)) return true;
                }
            } else if (name == argv[0] + ".papp") {
                return true;
            }
        }
        return false;
    };

    // Check AppRegistry first
    if (rt.apps().getApp(argv[0].c_str())) {
        for (auto& c : cmds_) {
            if (c.name == "run") { c.handler(ctx); return; }
        }
    }

    // Scan PATH directories recursively for .papp files
    for (const auto& dir : session.path) {
        if (findAppInDir(dir)) {
            for (auto& c : cmds_) {
                if (c.name == "run") { c.handler(ctx); return; }
            }
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

static std::string fmtSize(uint32_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024u * 1024u) {
        char buf[16]; std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0f); return buf;
    }
    char buf[16]; std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0f * 1024.0f)); return buf;
}

} // namespace

void registerCoreCliCommands(CliService& cli, Runtime& rt) {
    cli.setRuntime(rt);  // enable auto-launch fallback (Plan 57)
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
            out("INSTALLED:");
            bool anyInst = false;
            for (const auto& m : r->apps().list()) {
                if (m.type == AppType::Service) continue;
                std::string line = std::string("  ") + (m.name ? m.name : m.id ? m.id : "?");
                line += "  [" + std::string(runtimeTierName(m.runtimeTier)) + "]";
                if (m.kind == AppKind::Custom) line += "  custom";
                out(line);
                anyInst = true;
            }
            if (!anyInst) out("  (none)");
            out("SESSIONS:");
            for (auto& s : r->cliSessions().sessions())
                out("  #" + std::to_string(s->id) + "  cwd=" + s->cwd);
            out("PROCESSES:");
            const auto& procs = r->processes().list();
            if (procs.empty()) {
                out("  (none)");
            } else {
                for (auto* p : procs)
                    out(std::string("  ") + (p->finished() ? "[exited:" + std::to_string(p->exitCode()) + "]" : "[running]"));
            }
        });

    // ── Process execution (Plan 54) ───────────────────────────────────
    // Helper: launch one app and wait for it. Returns exit code.
    // stdout/stderr go to the given sinks; null sinks if omitted.
    auto launchOne = [r](const std::vector<std::string>& argv,
                         const std::string& cwd,
                         IOutputStream* outSink,
                         IOutputStream* errSink) -> int {
        IApp* app = r->apps().getApp(argv.empty() ? "" : argv[0].c_str());
        if (!app) return -1;

        ProcessSpec spec;
        spec.argv = argv;
        spec.cwd  = cwd;
        spec.stdout_ = outSink;
        spec.stderr_ = errSink;

        ProcessHost host(*r, *app, std::move(spec));
        r->processes().add(host);
        host.start();
        host.join();
        r->processes().remove(host);
        return host.exitCode();
    };

    cli.add("run", "run an app [run <app> args… | <appB> args…]",
        [launchOne](CliContext& c) {
            const auto& args = c.args; const auto& out = c.out;
            if (args.empty()) { out("usage: run <app> [args…]"); return; }

            // Find pipe separator
            auto pipeIt = std::find(args.begin(), args.end(), "|");
            bool hasPipe = (pipeIt != args.end());

            if (!hasPipe) {
                // Single process
                LineOutputStream lineOut(c.session.out);
                int ec = launchOne(args, c.session.cwd, &lineOut, &lineOut);
                c.session.lastExit = (ec < 0) ? 127 : ec;
                if (ec < 0) out("run: app not found: " + args[0]);
            } else {
                // Pipeline: A | B
                std::vector<std::string> leftArgs(args.begin(), pipeIt);
                std::vector<std::string> rightArgs(pipeIt + 1, args.end());

                if (leftArgs.empty() || rightArgs.empty()) {
                    out("run: invalid pipeline (empty left or right side)");
                    return;
                }

                Pipe pipe;
                LineOutputStream lineOut(c.session.out);

                // Launch left (A) — stdout → pipe writer
                int ecLeft = launchOne(leftArgs, c.session.cwd, &pipe.writer(), &lineOut);
                pipe.writer().close();  // signal EOF to reader

                // Launch right (B) — stdin → pipe reader, stdout → session
                if (ecLeft < 0) {
                    out("run: app not found: " + leftArgs[0]);
                    c.session.lastExit = 127;
                } else {
                    int ecRight = launchOne(rightArgs, c.session.cwd, &lineOut, &lineOut);
                    c.session.lastExit = (ecRight < 0) ? 127 : ecRight;
                    if (ecRight < 0) out("run: app not found: " + rightArgs[0]);
                }
            }
        });

    cli.add("echo", "print text (echo $? for last exit code)",
        [](CliContext& c) {
            if (!c.args.empty() && c.args[0] == "$?") {
                c.out(std::to_string(c.session.lastExit));
            } else {
                std::string line;
                for (size_t i = 0; i < c.args.size(); i++) {
                    if (i > 0) line += " ";
                    line += c.args[i];
                }
                c.out(line);
            }
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

    auto version = [r](CliContext& c) {
        const SystemInfo& si = r->info();
        c.out("fw:    " + si.firmwareVersion);
        c.out("build: " + si.buildVersion);
        c.out("board: " + std::string(r->board().name()) + " / " + si.platformName);
        if (auto* ota = r->container().resolve<IOtaUpdater>(); ota && ota->supported())
            c.out(std::string("slot:  ") + ota->runningSlot());  // active A/B slot
    };
    cli.add("version", "firmware version, build hash, active OTA slot", version);
    cli.add("ver", "alias of version", version);

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

    cli.add("power", "power control: power restart|shutdown|bootloader",
        [r](CliContext& c) {
            const auto& args = c.args; const auto& out = c.out;
            if (args.empty()) { out("usage: power restart|shutdown|bootloader"); return; }
            if (args[0] == "restart")    { out("restarting…");   r->requestRestart(); }
            else if (args[0] == "shutdown")   { out("shutting down…"); r->requestShutdown(); }
            else if (args[0] == "bootloader") { out("entering bootloader…"); r->requestBootloader(); }
            else out("unknown: " + args[0] + " (restart|shutdown|bootloader)");
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

    cli.add("ls", "list directory: ls [path]",
        [r](CliContext& c) {
            auto* fs = r->container().resolve<IFileSystem>();
            if (!fs) { c.out("ls: filesystem not available"); return; }
            std::string path = c.args.empty() ? c.session.cwd : resolvePath(c.session.cwd, c.args[0]);
            std::vector<FsEntry> es;
            if (!fs->list(path, es)) { c.out("ls: no such directory: " + path); return; }
            for (auto& e : es)
                c.out((e.isDir ? "d " : "- ") + e.name +
                      (e.isDir ? "" : "  " + fmtSize(e.size)));
        });

    cli.add("cat", "print file contents: cat <path>",
        [r](CliContext& c) {
            if (c.args.empty()) { c.out("usage: cat <path>"); return; }
            auto* fs = r->container().resolve<IFileSystem>();
            if (!fs) { c.out("cat: filesystem not available"); return; }
            std::string path = resolvePath(c.session.cwd, c.args[0]);
            std::vector<uint8_t> data;
            if (!fs->read(path, data)) { c.out("cat: no such file: " + path); return; }
            c.out(std::string((const char*)data.data(), data.size()));
        });

    cli.add("rm", "remove file or directory: rm <path>",
        [r](CliContext& c) {
            if (c.args.empty()) { c.out("usage: rm <path>"); return; }
            auto* fs = r->container().resolve<IFileSystem>();
            if (!fs) { c.out("rm: filesystem not available"); return; }
            std::string path = resolvePath(c.session.cwd, c.args[0]);
            c.out(fs->remove(path) ? "removed " + path : "rm: failed: " + path);
        });

    cli.add("mkdir", "create directory: mkdir <path>",
        [r](CliContext& c) {
            if (c.args.empty()) { c.out("usage: mkdir <path>"); return; }
            auto* fs = r->container().resolve<IFileSystem>();
            if (!fs) { c.out("mkdir: filesystem not available"); return; }
            std::string path = resolvePath(c.session.cwd, c.args[0]);
            c.out(fs->mkdir(path) ? "created " + path : "mkdir: failed: " + path);
        });

    cli.add("config", "config get|set|rm <ns> <key> [value]",
        [r](CliContext& c) {
            const auto& args = c.args; const auto& out = c.out;
            auto* cfg = r->container().resolve<IConfigStore>();
            if (!cfg) { out("config: not available"); return; }
            if (args.size() < 3) {
                out("usage:");
                out("  config get <ns> <key>");
                out("  config set <ns> <key> <value>");
                out("  config rm  <ns> <key>");
                return;
            }
            const std::string& sub = args[0];
            const std::string& ns  = args[1];
            const std::string& key = args[2];
            if (sub == "get") {
                std::string v;
                if (cfg->getString(ns.c_str(), key.c_str(), v))
                    out(ns + "/" + key + " = " + v);
                else
                    out(ns + "/" + key + ": not set");
            } else if (sub == "set") {
                if (args.size() < 4) { out("usage: config set <ns> <key> <value>"); return; }
                cfg->setString(ns.c_str(), key.c_str(), args[3]);
                out("set " + ns + "/" + key + " = " + args[3]);
                // Note: presentation settings (e.g. aether/theme) are owned by the
                // display server (ADR 0002). They apply on next boot, or live via
                // Settings → Display. The kernel CLI just persists the value here.
                if (ns == "aether" && key == "theme") {
                    r->view().requestRedraw();
                    out("(theme saved — applies on reboot or via Settings)");
                }
            } else if (sub == "rm") {
                out(cfg->remove(ns.c_str(), key.c_str())
                    ? "removed " + ns + "/" + key
                    : ns + "/" + key + ": not found");
            } else {
                out("unknown subcommand: " + sub);
            }
        });
}

} // namespace nema
