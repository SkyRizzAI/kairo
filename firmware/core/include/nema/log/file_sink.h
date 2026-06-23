#pragma once
#include "nema/log/log_sink.h"
#include <cstdio>
#include <string>

namespace nema {

// Appends log lines to a file (same format as ConsoleSink).
// Lazy-opens on the first write: if the path isn't available yet (e.g. SD card
// not mounted), it retries every kRetryInterval writes so the caller doesn't
// need to know when the filesystem becomes ready.
// NOTE: pass the real POSIX/VFS path, not the project Vfs virtual path.
// On ESP32, the SD card is mounted by ESP-IDF at /sdcard (default), not /sd.
class FileSink : public ILogSink {
public:
    explicit FileSink(std::string path);
    ~FileSink() override;
    void write(const LogEntry& entry) override;

private:
    std::string path_;
    FILE*       file_    = nullptr;
    int         retryIn_ = 0;

    static constexpr int kRetryInterval = 50;
};

} // namespace nema
