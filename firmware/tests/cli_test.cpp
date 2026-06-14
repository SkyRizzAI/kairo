// Host unit test for the CLI shell session core (Plan 44).
#include "nema/services/cli_service.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace nema;

static int fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); fail++; } \
                         else       std::printf("  ok:   %s\n", m); } while (0)

int main() {
    std::printf("== CLI shell session tests ==\n");

    CliService cli;

    // A command that exercises the context: echoes argv and the session cwd, and
    // can mutate the session (proves per-session state persists across lines).
    cli.add("echo", "echo args", [](CliContext& c) {
        std::string s;
        for (auto& a : c.args) s += a + " ";
        c.out("echo: " + s);
        c.out("cwd: " + c.session.cwd);
    });
    cli.add("setcwd", "set cwd", [](CliContext& c) {
        if (!c.args.empty()) c.session.cwd = c.args[0];   // stateful mutation
    });

    std::vector<std::string> outbuf;
    CliSession s;
    s.out = [&](const std::string& line) { outbuf.push_back(line); };

    // Dispatch + context carries args and the session's cwd.
    cli.execute("echo hello world", s);
    CHECK(outbuf.size() == 2, "command produced 2 output lines");
    CHECK(outbuf[0] == "echo: hello world ", "args passed via context");
    CHECK(outbuf[1] == "cwd: /", "default cwd is /");

    // Per-session state persists across execute() calls.
    cli.execute("setcwd /apps", s);
    outbuf.clear();
    cli.execute("echo x", s);
    CHECK(outbuf[1] == "cwd: /apps", "session cwd persisted across lines");

    // History accumulates, dedups immediate repeats, and is capped.
    CHECK(s.history.size() == 3, "history has 3 entries (echo, setcwd, echo)");
    cli.execute("echo y", s);                  // distinct → pushed (size 4)
    cli.execute("echo y", s);                  // immediate dupe → not re-pushed
    CHECK(s.history.back() == "echo y", "history tail is last command");
    CHECK(s.history.size() == 4, "immediate duplicate not added twice");
    for (int i = 0; i < 50; i++) cli.execute("cmd" + std::to_string(i), s);
    CHECK(s.history.size() == 32, "history ring capped at 32");

    // Unknown command reports an error on the session sink.
    outbuf.clear();
    cli.execute("nope", s);
    CHECK(!outbuf.empty() && outbuf[0].rfind("unknown command", 0) == 0,
          "unknown command reported");

    // reset() clears cwd + history (used on disconnect).
    s.reset();
    CHECK(s.cwd == "/" && s.history.empty(), "reset clears cwd + history");

    // Convenience execute(line, out): throwaway session, still dispatches.
    outbuf.clear();
    cli.execute("echo conv", [&](const std::string& l) { outbuf.push_back(l); });
    CHECK(outbuf.size() == 2 && outbuf[0] == "echo: conv ",
          "execute(line,out) convenience works");

    // Multi-session isolation (Plan 45): two sessions don't interfere.
    {
        CliSessionManager mgr;
        std::vector<std::string> a, b;
        CliSession& s1 = mgr.get(1);
        s1.out = [&](const std::string& l) { a.push_back(l); };
        CliSession& s2 = mgr.get(2);
        s2.out = [&](const std::string& l) { b.push_back(l); };
        CHECK(&mgr.get(1) == &s1, "get() returns the same session for an id");
        CHECK(mgr.sessions().size() == 2, "two distinct sessions tracked");

        cli.execute("setcwd /apps", s1);   // cd in session 1 only
        a.clear();
        cli.execute("echo z", s1);
        cli.execute("echo z", s2);
        CHECK(a.back() == "cwd: /apps", "session 1 sees its own cwd");
        CHECK(b.back() == "cwd: /", "session 2 unaffected by session 1's cd");

        mgr.remove(1);
        CHECK(mgr.sessions().size() == 1, "remove() drops one session");
        mgr.clear();
        CHECK(mgr.sessions().empty(), "clear() drops all sessions");
    }

    std::printf(fail == 0 ? "== ALL PASS ==\n" : "== FAILURES ==\n");
    return fail == 0 ? 0 : 1;
}
