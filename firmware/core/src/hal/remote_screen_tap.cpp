#include "nema/hal/remote_screen_tap.h"
#include <cstring>

namespace nema {

void RemoteScreenTap::init(IDisplayDriver& inner, LinkService& link) {
    inner_ = &inner;
    link_  = &link;
    ensureShadow();
}

void RemoteScreenTap::ensureShadow() {
    if (!inner_) return;
    uint16_t w = inner_->width(), h = inner_->height();
    if (w != w_ || h != h_ || shadow_.size() != (size_t)w * h) {
        w_ = w; h_ = h;
        shadow_.assign((size_t)w_ * h_, 0);
    }
}

void RemoteScreenTap::drawPixel(uint16_t x, uint16_t y, bool on) {
    if (inner_) inner_->drawPixel(x, y, on);
    if (x < w_ && y < h_) shadow_[(size_t)y * w_ + x] = on ? 1 : 0;
}

void RemoteScreenTap::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) {
    if (inner_) inner_->fillRect(x, y, w, h, on);
    for (uint16_t yy = y; yy < y + h && yy < h_; yy++)
        for (uint16_t xx = x; xx < x + w && xx < w_; xx++)
            shadow_[(size_t)yy * w_ + xx] = on ? 1 : 0;
}

void RemoteScreenTap::clear(bool on) {
    // Frame start: re-sync the shadow to the inner driver's current dims so a
    // live rotation (dim swap) is picked up before this frame's pixels land.
    ensureShadow();
    if (inner_) inner_->clear(on);
    if (!shadow_.empty()) std::memset(shadow_.data(), on ? 1 : 0, shadow_.size());
}

void RemoteScreenTap::invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (inner_) inner_->invertRect(x, y, w, h);
    for (uint16_t yy = y; yy < y + h && yy < h_; yy++)
        for (uint16_t xx = x; xx < x + w && xx < w_; xx++) {
            auto& p = shadow_[(size_t)yy * w_ + xx];
            p = p ? 0 : 1;
        }
}

void RemoteScreenTap::blitRgb565(const uint8_t* buf, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (inner_) inner_->blitRgb565(buf, x, y, w, h);   // colour path: forward only, not streamed
}

void RemoteScreenTap::flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) {
    if (inner_) inner_->flushBuffer(buf, w, h);
    ensureShadow();
    if (w == w_ && h == h_ && shadow_.size() == (size_t)w * h)
        std::memcpy(shadow_.data(), buf, shadow_.size());
    streamFrame();
}

void RemoteScreenTap::flush() {
    if (inner_) inner_->flush();
    streamFrame();
}

void RemoteScreenTap::streamFrame() {
    // Opt-in: only stream when the connected host actually wants the mirror
    // (Forge web /remote does; the file/CLI tooling does not). This stops the
    // screen flood from starving the inbound file/CLI path on the USB RX task.
    if (!link_ || !link_->ready() || !link_->screenWanted() || shadow_.empty()) return;
    // Downsample by the UI scale (lossless on integer-scaled content): 1 sample per f×f
    // block → f² fewer bytes on the wire. The host renders the logical W×H. (Plan 93)
    const uint8_t* src = shadow_.data();
    uint16_t sw = w_, sh = h_;
    if (downscale_ > 1) {
        sw = (uint16_t)(w_ / downscale_);
        sh = (uint16_t)(h_ / downscale_);
        ds_.resize((size_t)sw * sh);
        for (uint16_t y = 0; y < sh; y++)
            for (uint16_t x = 0; x < sw; x++)
                ds_[(size_t)y * sw + x] =
                    shadow_[(size_t)(y * downscale_) * w_ + (size_t)(x * downscale_)];
        src = ds_.data();
    }
    // Skip-if-unchanged (Plan 93): only stream when the frame actually differs from the
    // last one sent. A static screen → zero traffic; an animation → still sent every frame
    // (each render differs). Combined with downscale this keeps BLE light enough to share
    // the radio/RAM with WiFi. requestResend() clears prev_ to force a frame to a new viewer.
    const size_t npx = (size_t)sw * sh;
    if (prev_.size() == npx && std::memcmp(prev_.data(), src, npx) == 0) return;
    prev_.assign(src, src + npx);
    // Payload: [w:2 LE][h:2 LE][RLE 1-bit bytes...]
    auto rle = plp::rleEncode(src, npx);
    payload_.clear();
    payload_.reserve(4 + rle.size());
    payload_.push_back((uint8_t)(sw & 0xff));
    payload_.push_back((uint8_t)(sw >> 8));
    payload_.push_back((uint8_t)(sh & 0xff));
    payload_.push_back((uint8_t)(sh >> 8));
    payload_.insert(payload_.end(), rle.begin(), rle.end());
    link_->send(plp::Channel::Screen, payload_.data(), payload_.size(), plp::Flags::Compressed);
}

// Plan 92 Fase B — forward to the inner driver and, on change, tell the host the
// new colour palette so the Forge mirror recolours from the device's setting.
void RemoteScreenTap::setPalette(uint16_t fg, uint16_t bg) {
    if (inner_) inner_->setPalette(fg, bg);
    if (fg != palFg_ || bg != palBg_) {
        palFg_ = fg; palBg_ = bg;
        sendPalette();
    }
}

void RemoteScreenTap::sendPalette() {
    if (!link_ || !link_->ready()) return;
    // SYSTEM channel, SysOp::SetPalette (0x03): [op][fg:2 LE][bg:2 LE] RGB565.
    uint8_t buf[5] = {
        0x03,
        (uint8_t)(palFg_ & 0xff), (uint8_t)(palFg_ >> 8),
        (uint8_t)(palBg_ & 0xff), (uint8_t)(palBg_ >> 8),
    };
    link_->send(plp::Channel::System, buf, sizeof(buf));
}

void RemoteScreenTap::sleep() { if (inner_) inner_->sleep(); }
void RemoteScreenTap::wake()  { if (inner_) inner_->wake();  }

} // namespace nema
