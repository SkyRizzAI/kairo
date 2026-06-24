#include "nema/ui/lazy_dir_loader.h"
#include <algorithm>

namespace aether::ui {

void LazyDirLoader::open(nema::IFileSystem& fs, const std::string& path, int batchSize) {
    cancel();  // stop any previous load

    fs_    = &fs;
    path_  = path;
    batch_ = (batchSize > 0) ? batchSize : DEFAULT_BATCH;

    // Reset app-thread state
    snapshot_.clear();
    revealed_  = 0;
    snapped_   = false;
    state_     = State::Loading;

    // Reset shared state
    {
        std::lock_guard<std::mutex> lk(mutex_);
        raw_.clear();
        rawReady_ = false;
        rawError_ = false;
    }

    thread_.start({"LazyDirLoader", 4096, 3, -1}, threadEntry, this);
}

void LazyDirLoader::cancel() {
    if (!thread_.running()) return;
    thread_.requestStop();
    thread_.join();
    if (state_ == State::Loading) state_ = State::Idle;
}

// static
void LazyDirLoader::threadEntry(void* self) {
    static_cast<LazyDirLoader*>(self)->runLoad();
}

void LazyDirLoader::runLoad() {
    std::vector<nema::FsEntry> fsEntries;
    bool ok = fs_->list(path_, fsEntries);

    if (thread_.shouldStop()) return;

    std::vector<Entry> result;
    if (ok) {
        result.reserve(fsEntries.size());
        for (auto& fe : fsEntries) {
            result.push_back({fe.name, fe.isDir, fe.size});
        }
        // Dirs first, then alphabetical within each group.
        std::stable_sort(result.begin(), result.end(), [](const Entry& a, const Entry& b) {
            if (a.isDir != b.isDir) return a.isDir > b.isDir;
            return a.name < b.name;
        });
    }

    std::lock_guard<std::mutex> lk(mutex_);
    raw_      = std::move(result);
    rawReady_ = true;
    rawError_ = !ok;
}

bool LazyDirLoader::poll() {
    if (state_ == State::Idle || state_ == State::Error) return false;

    // First poll after bg thread finishes: take snapshot.
    if (!snapped_) {
        bool ready = false;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            ready = rawReady_;
            if (ready) snapshot_ = std::move(raw_);
        }
        if (!ready) {
            // Still loading — count() stays at 0, build() shows empty list or
            // the caller can check loading() to show a spinner screen instead.
            return false;
        }
        snapped_ = true;
        state_   = rawError_ ? State::Error : State::Done;
        // Reveal first batch immediately.
        revealed_ = std::min(batch_, (int)snapshot_.size());
        return true;  // trigger rebuild
    }

    // Progressive reveal: expose batch_ more entries per poll() call.
    if (revealed_ < (int)snapshot_.size()) {
        revealed_ = std::min(revealed_ + batch_, (int)snapshot_.size());
        return true;
    }

    return false;
}

} // namespace aether::ui
