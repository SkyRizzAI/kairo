#include "nema/esp32/sd_fat_filesystem.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstdio>
#include <unistd.h>

namespace nema {

// Each SD/SPI transaction makes ESP-IDF allocate a contiguous INTERNAL DMA buffer. Under
// heavy concurrent load (BLE controller + WiFi) that pool can be exhausted, and the IDF
// SD path then dereferences the failed allocation and PANICS the whole device (Plan 93 —
// the "cd /sd over BLE" crash). Refuse SD I/O gracefully when DMA RAM is too low to be
// safe, so the caller gets a clean failure instead of a reboot. ~8 KB covers the priv
// transfer buffer (max_transfer_sz=4000) plus descriptors with margin.
static bool sdDmaReady() {
    constexpr size_t kMinDmaBlock = 8 * 1024;
    return heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL) >= kMinDmaBlock;
}

bool SdFatFileSystem::begin(const SdSpiConfig& cfg, const char* basePath) {
    if (mounted_) return true;
    base_ = basePath;

    auto host_id = (spi_host_device_t)cfg.hostId;

    spi_bus_config_t bus = {};
    bus.mosi_io_num     = cfg.mosi;
    bus.miso_io_num     = cfg.miso;
    bus.sclk_io_num     = cfg.sclk;
    bus.quadwp_io_num   = -1;
    bus.quadhd_io_num   = -1;
    bus.max_transfer_sz = 4000;
    if (spi_bus_initialize(host_id, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = host_id;

    sdspi_device_config_t dev = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev.gpio_cs = (gpio_num_t)cfg.cs;
    dev.host_id = host_id;

    esp_vfs_fat_mount_config_t mount = {};
    mount.format_if_mount_failed = false;   // never reformat the user's card
    mount.max_files              = 5;
    mount.allocation_unit_size   = 16 * 1024;

    sdmmc_card_t* card = nullptr;
    esp_err_t err = esp_vfs_fat_sdspi_mount(base_.c_str(), &host, &dev, &mount, &card);
    if (err != ESP_OK) {
        spi_bus_free(host_id);   // release the bus so a later retry can re-init
        return false;
    }
    card_    = card;
    mounted_ = true;
    return true;
}

std::string SdFatFileSystem::real(const std::string& vpath) const {
    if (vpath.empty() || vpath == "/") return base_;
    return base_ + (vpath[0] == '/' ? vpath : "/" + vpath);
}

void SdFatFileSystem::mkdirsFor(const std::string& realPath) {
    size_t start = base_.size() + 1;
    for (size_t i = start; i < realPath.size(); i++) {
        if (realPath[i] == '/') ::mkdir(realPath.substr(0, i).c_str(), 0777);
    }
}

bool SdFatFileSystem::list(const std::string& path, std::vector<FsEntry>& out) {
    if (!mounted_ || !sdDmaReady()) return false;   // refuse vs crash under low DMA RAM
    std::string rp = real(path);
    DIR* d = opendir(rp.c_str());
    if (!d) return false;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        const char* n = ent->d_name;
        if (n[0] == '.' && (n[1] == 0 || (n[1] == '.' && n[2] == 0))) continue;
        // Use d_type to check directory flag without a stat() call when possible.
        // Fall back to stat() only when d_type is DT_UNKNOWN (rare on FAT).
        bool isDir = (ent->d_type == DT_DIR);
        uint32_t size = 0;
        if (ent->d_type == DT_UNKNOWN || (!isDir)) {
            struct stat st = {};
            std::string child = rp + "/" + n;
            if (stat(child.c_str(), &st) == 0) {
                isDir = S_ISDIR(st.st_mode);
                size  = (uint32_t)st.st_size;
            }
        }
        // Feed the task watchdog only if THIS task is subscribed (inline list now
        // runs on cdc_rx, which isn't a TWDT task → avoid "task not found" spam).
        if (esp_task_wdt_status(nullptr) == ESP_OK) esp_task_wdt_reset();
        out.push_back({std::string(n), isDir, size});
    }
    closedir(d);
    return true;
}

bool SdFatFileSystem::read(const std::string& path, std::vector<uint8_t>& out) {
    if (!mounted_ || !sdDmaReady()) return false;   // refuse vs crash under low DMA RAM
    FILE* f = fopen(real(path).c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    if (n > 0) {
        size_t got = fread(out.data(), 1, (size_t)n, f);
        out.resize(got);
    }
    fclose(f);
    return true;
}

bool SdFatFileSystem::write(const std::string& path, const uint8_t* data, size_t len) {
    if (!mounted_ || !sdDmaReady()) {
        if (log_) log_->warn("SdFatFS", "write: not mounted / low DMA RAM", {{"path", path}});
        return false;
    }
    std::string rp = real(path);
    mkdirsFor(rp);
    FILE* f = fopen(rp.c_str(), "wb");
    if (!f) {
        if (log_) log_->warn("SdFatFS", "write: fopen failed",
                             {{"path", rp}, {"errno", std::to_string(errno)}});
        return false;
    }
    size_t wrote = len ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    if (wrote != len) {
        if (log_) log_->warn("SdFatFS", "write: partial write",
                             {{"path", rp}, {"wrote", std::to_string(wrote)},
                              {"expected", std::to_string(len)}, {"errno", std::to_string(errno)}});
        return false;
    }
    return true;
}

// ── Streaming write (Plan 88 R5): one FILE* held open across chunks, no full-file
//    RAM buffer. `offset` seeking makes a retried chunk overwrite in place. ──
bool SdFatFileSystem::writeStreamBegin(const std::string& path) {
    if (!mounted_) return false;
    if (stream_) { fclose((FILE*)stream_); stream_ = nullptr; }   // drop a stale stream
    std::string rp = real(path);
    mkdirsFor(rp);
    FILE* f = fopen(rp.c_str(), "wb");
    if (!f) {
        if (log_) log_->warn("SdFatFS", "stream: fopen failed",
                             {{"path", rp}, {"errno", std::to_string(errno)}});
        return false;
    }
    stream_ = f;
    return true;
}

bool SdFatFileSystem::writeStreamChunk(uint32_t offset, const uint8_t* data, size_t len) {
    FILE* f = (FILE*)stream_;
    if (!f || !sdDmaReady()) return false;   // refuse vs crash under low DMA RAM
    if (fseek(f, (long)offset, SEEK_SET) != 0) return false;
    size_t wrote = len ? fwrite(data, 1, len, f) : 0;
    return wrote == len;
}

bool SdFatFileSystem::writeStreamEnd() {
    FILE* f = (FILE*)stream_;
    if (!f) return false;
    stream_ = nullptr;
    bool ok = (fclose(f) == 0);
    if (!ok && log_) log_->warn("SdFatFS", "stream: close failed", {{"errno", std::to_string(errno)}});
    return ok;
}

void SdFatFileSystem::writeStreamAbort() {
    if (stream_) { fclose((FILE*)stream_); stream_ = nullptr; }
}

bool SdFatFileSystem::mkdir(const std::string& path) {
    if (!mounted_ || path == "/") return false;
    std::string rp = real(path);
    mkdirsFor(rp);
    return ::mkdir(rp.c_str(), 0777) == 0;
}

bool SdFatFileSystem::remove(const std::string& path) {
    if (!mounted_ || path == "/") return false;
    std::string rp = real(path);
    struct stat st = {};
    if (stat(rp.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode) ? (rmdir(rp.c_str()) == 0) : (unlink(rp.c_str()) == 0);
}

bool SdFatFileSystem::rename(const std::string& src, const std::string& dst) {
    if (!mounted_) return false;
    std::string rs = real(src), rd = real(dst);
    mkdirsFor(rd);
    return ::rename(rs.c_str(), rd.c_str()) == 0;
}

StatVfs SdFatFileSystem::statvfs() const {
    if (!mounted_) return {};
    uint64_t total = 0, free = 0;
    if (esp_vfs_fat_info(base_.c_str(), &total, &free) != ESP_OK) return {};
    return {total, free};
}

} // namespace nema
