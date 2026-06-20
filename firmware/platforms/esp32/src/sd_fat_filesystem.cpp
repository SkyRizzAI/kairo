#include "nema/esp32/sd_fat_filesystem.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <unistd.h>

namespace nema {

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
    if (!mounted_) return false;
    std::string rp = real(path);
    DIR* d = opendir(rp.c_str());
    if (!d) return false;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        const char* n = ent->d_name;
        if (n[0] == '.' && (n[1] == 0 || (n[1] == '.' && n[2] == 0))) continue;
        struct stat st = {};
        std::string child = rp + "/" + n;
        bool isDir = false;
        uint32_t size = 0;
        if (stat(child.c_str(), &st) == 0) {
            isDir = S_ISDIR(st.st_mode);
            size = (uint32_t)st.st_size;
        }
        out.push_back({std::string(n), isDir, size});
    }
    closedir(d);
    return true;
}

bool SdFatFileSystem::read(const std::string& path, std::vector<uint8_t>& out) {
    if (!mounted_) return false;
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
    if (!mounted_) return false;
    std::string rp = real(path);
    mkdirsFor(rp);
    FILE* f = fopen(rp.c_str(), "wb");
    if (!f) return false;
    size_t wrote = len ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    return wrote == len;
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

} // namespace nema
