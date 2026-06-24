#pragma once
#include "nema/hal/filesystem.h"
#include "nema/thread.h"
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>

namespace aether::ui {

// ── LazyDirLoader ─────────────────────────────────────────────────────────────
//
// Reads a directory from IFileSystem on a background thread so the UI stays
// responsive. Entries are revealed to the app thread in batches (REVEAL_BATCH
// per poll() call) to produce a progressive-loading effect — like Flipper Zero's
// "..." skeleton, but driven by real data as it becomes available.
//
// Usage pattern (in a ComponentApp or ComponentScreen):
//
//   // As screen member:
//   LazyDirLoader   loader_;
//   VirtualListState vlst_;
//
//   void onResume() override {
//       loader_.open(rt_.fs(), "/sd/music");
//   }
//
//   bool onTick(AppContext& ctx) override {
//       return loader_.poll();   // returns true → state changed → rebuild
//   }
//   uint32_t tickIntervalMs() const override { return 80; }
//
//   UiNode* build(NodeArena& a, AppContext& ctx) override {
//       int n = loader_.count();
//       return VirtualList(a, vlst_, n, 10,
//           [](NodeArena& a, int i, bool focused, void* ud) {
//               auto* self = (MyScreen*)ud;
//               const auto* e = self->loader_.entryAt(i);
//               if (!e) return SkeletonRow(a);
//               return ListItemRow(a, {.label = e->name.c_str(),
//                                      .isDir = e->isDir, ...});
//           }, this);
//   }
//
// Thread safety: open()/cancel() and poll()/count()/entryAt()/state()
// must only be called from the app/screen thread. The background thread
// only writes to internal state (mutex-protected).
//
class LazyDirLoader {
public:
    enum class State : uint8_t { Idle, Loading, Done, Error };

    struct Entry {
        std::string name;
        bool        isDir = false;
        uint32_t    size  = 0;
    };

    LazyDirLoader() = default;
    ~LazyDirLoader() { cancel(); }

    LazyDirLoader(const LazyDirLoader&)            = delete;
    LazyDirLoader& operator=(const LazyDirLoader&) = delete;

    // Start (or restart) a directory load. Cancels any in-progress load.
    // `batchSize` = entries revealed per poll() call (default 20).
    void open(nema::IFileSystem& fs, const std::string& path, int batchSize = 20);

    // Stop a background load in progress. Blocks briefly until the thread exits.
    void cancel();

    // ── App-thread read API ───────────────────────────────────────────────────

    // Call from onTick(). Returns true if the visible count changed since last
    // call (new entries revealed OR load completed/failed) — signal to rebuild.
    bool poll();

    // Number of entries currently revealed to the app thread. Grows each poll()
    // call while loading/done. Use as `totalCount` in VirtualList().
    int count() const { return revealed_; }

    // Total entries read from the filesystem (>= count(), set when Done).
    int total() const { return (int)snapshot_.size(); }

    // Entry at index (0-based, must be < count()). Returns nullptr if index
    // is out of range — callers should show a SkeletonRow in that case.
    const Entry* entryAt(int index) const {
        if (index < 0 || index >= revealed_) return nullptr;
        return &snapshot_[(size_t)index];
    }

    State       state() const { return state_; }
    bool        idle()    const { return state_ == State::Idle; }
    bool        loading() const { return state_ == State::Loading; }
    bool        done()    const { return state_ == State::Done; }
    bool        error()   const { return state_ == State::Error; }

    // Current path being (or last) loaded.
    const std::string& path() const { return path_; }

private:
    static constexpr int DEFAULT_BATCH = 20;

    // Background thread entry.
    static void threadEntry(void* self);
    void        runLoad();

    nema::Thread thread_;
    nema::IFileSystem* fs_   = nullptr;
    std::string        path_;
    int                batch_ = DEFAULT_BATCH;

    // Protected by mutex_ — written by bg thread, read by poll().
    mutable std::mutex      mutex_;
    std::vector<Entry>      raw_;        // all entries from list()
    bool                    rawReady_  = false;
    bool                    rawError_  = false;

    // App-thread snapshot (no lock needed after transfer).
    std::vector<Entry> snapshot_;
    int                revealed_  = 0;
    State              state_     = State::Idle;
    bool               snapped_   = false;   // true after snapshot taken from raw_
};

} // namespace aether::ui
