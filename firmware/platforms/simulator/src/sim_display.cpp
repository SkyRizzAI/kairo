#include "kairo/sim/sim_display.h"
#include "kairo/sim/bridge.h"
#include "kairo/log/logger.h"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace kairo {

// Simple base64 encoder — no external deps
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64Encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];
        out += B64[(n >> 18) & 63];
        out += B64[(n >> 12) & 63];
        out += (i + 1 < len) ? B64[(n >> 6) & 63] : '=';
        out += (i + 2 < len) ? B64[n & 63] : '=';
    }
    return out;
}

SimDisplay::~SimDisplay() { delete[] buf_; }

void SimDisplay::init(Logger& log, TelemetryBridge& bridge) {
    log_    = &log;
    bridge_ = &bridge;

    const char* envW = std::getenv("KAIRO_SIM_W");
    const char* envH = std::getenv("KAIRO_SIM_H");
    int w = envW ? std::atoi(envW) : 264;
    int h = envH ? std::atoi(envH) : 176;
    // Clamp to a sane range
    if (w < 64)   w = 64;
    if (w > 1024) w = 1024;
    if (h < 32)   h = 32;
    if (h > 768)  h = 768;
    w_ = (uint16_t)w;
    h_ = (uint16_t)h;

    delete[] buf_;
    buf_ = new uint8_t[(size_t)w_ * h_]();
}

void SimDisplay::start() {
    char res[16];
    std::snprintf(res, sizeof(res), "%dx%d", w_, h_);
    log_->info("SimDisplay", "started", {{"res", res}, {"mode", "1-bit"}});
}

void SimDisplay::stop() {
    log_->info("SimDisplay", "stopped");
}

void SimDisplay::drawPixel(uint16_t x, uint16_t y, bool on) {
    if (x >= w_ || y >= h_) return;
    buf_[(size_t)y * w_ + x] = on ? 1 : 0;
}

void SimDisplay::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) {
    for (uint16_t row = y; row < y + h && row < h_; row++) {
        uint16_t x1 = x, x2 = x + w;
        if (x2 > w_) x2 = w_;
        if (x1 < x2) std::memset(buf_ + (size_t)row * w_ + x1, on ? 1 : 0, x2 - x1);
    }
}

void SimDisplay::clear(bool on) {
    std::memset(buf_, on ? 1 : 0, (size_t)w_ * h_);
}

void SimDisplay::invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    for (uint16_t row = y; row < y + h && row < h_; row++)
        for (uint16_t col = x; col < x + w && col < w_; col++)
            buf_[(size_t)row * w_ + col] ^= 1;
}

void SimDisplay::flush() {
    // Emit as JSON line: {"type":"frame","width":W,"height":H,"data":"<base64>"}
    std::string enc = base64Encode(buf_, (size_t)w_ * h_);
    nlohmann::json j;
    j["type"]   = "frame";
    j["width"]  = w_;
    j["height"] = h_;
    j["data"]   = enc;
    simEmit(j.dump());   // frame written from GuiService thread — must be locked
}

void SimDisplay::sleep() { simEmit(R"({"type":"display_sleep"})"); }
void SimDisplay::wake()  { simEmit(R"({"type":"display_wake"})");  }

} // namespace kairo
