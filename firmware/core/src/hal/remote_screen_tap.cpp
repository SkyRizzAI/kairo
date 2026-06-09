#include "kairo/hal/remote_screen_tap.h"
#include <cstring>

namespace kairo {

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
    if (!link_ || !link_->ready() || shadow_.empty()) return;
    // Payload: [w:2 LE][h:2 LE][RLE 1-bit bytes...]
    auto rle = klp::rleEncode(shadow_.data(), shadow_.size());
    payload_.clear();
    payload_.reserve(4 + rle.size());
    payload_.push_back((uint8_t)(w_ & 0xff));
    payload_.push_back((uint8_t)(w_ >> 8));
    payload_.push_back((uint8_t)(h_ & 0xff));
    payload_.push_back((uint8_t)(h_ >> 8));
    payload_.insert(payload_.end(), rle.begin(), rle.end());
    link_->send(klp::Channel::Screen, payload_.data(), payload_.size(), klp::Flags::Compressed);
}

void RemoteScreenTap::sleep() { if (inner_) inner_->sleep(); }
void RemoteScreenTap::wake()  { if (inner_) inner_->wake();  }

} // namespace kairo
