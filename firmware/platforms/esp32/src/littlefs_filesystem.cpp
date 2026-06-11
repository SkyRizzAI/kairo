#include "kairo/esp32/littlefs_filesystem.h"
#include "esp_littlefs.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <unistd.h>

namespace kairo {

bool LittleFsFileSystem::begin(const char* partitionLabel, const char* basePath) {
    base_ = basePath;
    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = base_.c_str();
    conf.partition_label = partitionLabel;
    conf.format_if_mount_failed = true;   // first boot: format the blank partition
    conf.dont_mount = false;
    mounted_ = (esp_vfs_littlefs_register(&conf) == ESP_OK);
    return mounted_;
}

std::string LittleFsFileSystem::real(const std::string& vpath) const {
    if (vpath.empty() || vpath == "/") return base_;
    return base_ + (vpath[0] == '/' ? vpath : "/" + vpath);
}

void LittleFsFileSystem::mkdirsFor(const std::string& realPath) {
    // mkdir each ancestor of realPath (skip the base_ prefix, which is the mount).
    size_t start = base_.size() + 1;
    for (size_t i = start; i < realPath.size(); i++) {
        if (realPath[i] == '/') ::mkdir(realPath.substr(0, i).c_str(), 0777);
    }
}

bool LittleFsFileSystem::list(const std::string& path, std::vector<FsEntry>& out) {
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

bool LittleFsFileSystem::read(const std::string& path, std::vector<uint8_t>& out) {
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

bool LittleFsFileSystem::write(const std::string& path, const uint8_t* data, size_t len) {
    if (!mounted_) return false;
    std::string rp = real(path);
    mkdirsFor(rp);
    FILE* f = fopen(rp.c_str(), "wb");
    if (!f) return false;
    size_t wrote = len ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    return wrote == len;
}

bool LittleFsFileSystem::mkdir(const std::string& path) {
    if (!mounted_ || path == "/") return false;
    std::string rp = real(path);
    mkdirsFor(rp);
    return ::mkdir(rp.c_str(), 0777) == 0;
}

bool LittleFsFileSystem::remove(const std::string& path) {
    if (!mounted_ || path == "/") return false;
    std::string rp = real(path);
    struct stat st = {};
    if (stat(rp.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode) ? (rmdir(rp.c_str()) == 0) : (unlink(rp.c_str()) == 0);
}

} // namespace kairo
