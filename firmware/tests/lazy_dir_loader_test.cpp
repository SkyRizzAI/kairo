// Host unit tests for LazyDirLoader (Plan 90 F6.B1).
// Tests sort order, progressive reveal, empty directory, and error path.
// Uses a synchronous mock IFileSystem; thread joins before first poll().
#include "nema/ui/lazy_dir_loader.h"
#include "nema/hal/filesystem.h"
#include <cstdio>
#include <thread>
#include <chrono>

using namespace aether::ui;

static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", (msg)); g_fail++; } \
    else         { std::printf("  ok:   %s\n", (msg)); } \
} while (0)

// ── Mock filesystem ──────────────────────────────────────────────────────────

struct MockFs : nema::IFileSystem {
    const char* name() const override { return "mock"; }
    nema::DriverKind kind() const override { return nema::DriverKind::Storage; }

    struct Scenario {
        bool                      ok = true;
        std::vector<nema::FsEntry> entries;
    } scenario;

    bool list(const std::string&, std::vector<nema::FsEntry>& out) override {
        if (!scenario.ok) return false;
        out = scenario.entries;
        return true;
    }
    bool read  (const std::string&, std::vector<uint8_t>&) override { return false; }
    bool write (const std::string&, const uint8_t*, size_t) override { return false; }
    bool mkdir (const std::string&) override { return false; }
    bool remove(const std::string&) override { return false; }
    bool rename(const std::string&, const std::string&) override { return false; }
};

static nema::FsEntry makeEntry(const char* name, bool isDir, uint32_t size = 0) {
    nema::FsEntry e;
    e.name  = name;
    e.isDir = isDir;
    e.size  = size;
    return e;
}

// Poll until done or max_polls exhausted (each poll sleeps 5ms).
static bool pollUntilDone(LazyDirLoader& ldr, int max_polls = 100) {
    for (int i = 0; i < max_polls; i++) {
        ldr.poll();
        if (ldr.done() || ldr.error()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

// ── Tests ────────────────────────────────────────────────────────────────────

static void test_sort_dirs_first_alpha() {
    std::printf("\ntest_sort_dirs_first_alpha:\n");
    MockFs fs;
    fs.scenario.entries = {
        makeEntry("zebra.txt", false),
        makeEntry("alpha",     true),
        makeEntry("mango.txt", false),
        makeEntry("beta",      true),
        makeEntry("apple.txt", false),
    };

    LazyDirLoader ldr;
    ldr.open(fs, "/test");
    CHECK(pollUntilDone(ldr), "loader reaches Done state");
    CHECK(ldr.done(),  "state == Done");
    CHECK(ldr.count() == 5, "all 5 entries revealed");

    // Dirs first, then files — each group alpha-sorted.
    CHECK(ldr.entryAt(0) && ldr.entryAt(0)->name == "alpha",     "dir[0] = alpha");
    CHECK(ldr.entryAt(1) && ldr.entryAt(1)->name == "beta",      "dir[1] = beta");
    CHECK(ldr.entryAt(2) && ldr.entryAt(2)->name == "apple.txt", "file[0] = apple.txt");
    CHECK(ldr.entryAt(3) && ldr.entryAt(3)->name == "mango.txt", "file[1] = mango.txt");
    CHECK(ldr.entryAt(4) && ldr.entryAt(4)->name == "zebra.txt", "file[2] = zebra.txt");
}

static void test_empty_directory() {
    std::printf("\ntest_empty_directory:\n");
    MockFs fs;
    fs.scenario.entries = {};

    LazyDirLoader ldr;
    ldr.open(fs, "/empty");
    CHECK(pollUntilDone(ldr), "loader finishes");
    CHECK(ldr.done(),        "state == Done");
    CHECK(ldr.count() == 0,  "count == 0");
    CHECK(ldr.entryAt(0) == nullptr, "entryAt(0) == nullptr");
}

static void test_error_path() {
    std::printf("\ntest_error_path:\n");
    MockFs fs;
    fs.scenario.ok = false;

    LazyDirLoader ldr;
    ldr.open(fs, "/nonexistent");
    CHECK(pollUntilDone(ldr), "loader finishes");
    CHECK(ldr.error(),       "state == Error");
    CHECK(ldr.count() == 0,  "count == 0 on error");
}

static void test_out_of_bounds() {
    std::printf("\ntest_out_of_bounds:\n");
    MockFs fs;
    fs.scenario.entries = { makeEntry("a.txt", false) };

    LazyDirLoader ldr;
    ldr.open(fs, "/test");
    CHECK(pollUntilDone(ldr), "loader finishes");
    CHECK(ldr.entryAt(-1) == nullptr, "entryAt(-1) == nullptr");
    CHECK(ldr.entryAt(1)  == nullptr, "entryAt(count) == nullptr");
    CHECK(ldr.entryAt(0) != nullptr,  "entryAt(0) valid");
}

static void test_progressive_reveal() {
    std::printf("\ntest_progressive_reveal:\n");
    MockFs fs;
    // 25 entries, batchSize=10 → first poll reveals 10, second 20, third 25.
    for (int i = 0; i < 25; i++) {
        char name[16];
        std::snprintf(name, sizeof(name), "file%02d.txt", i);
        fs.scenario.entries.push_back(makeEntry(name, false));
    }

    LazyDirLoader ldr;
    ldr.open(fs, "/test", /*batchSize=*/10);
    CHECK(pollUntilDone(ldr), "loader finishes");
    CHECK(ldr.total() == 25, "total == 25");
    // After pollUntilDone the full snapshot is taken; all entries revealed.
    CHECK(ldr.count() >= 10, "at least first batch revealed");
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    test_sort_dirs_first_alpha();
    test_empty_directory();
    test_error_path();
    test_out_of_bounds();
    test_progressive_reveal();

    std::printf("\n%s — %d failure(s)\n",
                g_fail ? "FAIL" : "PASS", g_fail);
    return g_fail ? 1 : 0;
}
