#include "kairo/link/klp_codec.h"

namespace kairo::klp {

static constexpr size_t HEADER = 5; // magic + chan + flags + len(2)

uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
    return crc;
}

void encodeFrame(std::vector<uint8_t>& out, uint8_t channel,
                 const uint8_t* payload, size_t len, uint8_t flags) {
    out.clear();
    out.resize(HEADER + len + 1);
    out[0] = MAGIC;
    out[1] = channel;
    out[2] = flags;
    out[3] = (uint8_t)(len & 0xff);
    out[4] = (uint8_t)((len >> 8) & 0xff);
    for (size_t i = 0; i < len; i++) out[HEADER + i] = payload[i];
    out[HEADER + len] = crc8(out.data(), HEADER + len);
}

std::vector<Frame> FrameParser::push(const uint8_t* data, size_t len) {
    buf_.insert(buf_.end(), data, data + len);

    std::vector<Frame> frames;
    size_t off = 0;
    while (off < buf_.size()) {
        if (buf_[off] != MAGIC) { off++; continue; }          // scan for sync byte
        if (buf_.size() - off < HEADER) break;                 // wait for header
        size_t plen = (size_t)buf_[off + 3] | ((size_t)buf_[off + 4] << 8);
        size_t total = HEADER + plen + 1;
        if (buf_.size() - off < total) break;                  // wait for full frame
        uint8_t crc = buf_[off + HEADER + plen];
        if (crc8(&buf_[off], HEADER + plen) == crc) {
            Frame f;
            f.channel = buf_[off + 1];
            f.flags   = buf_[off + 2];
            f.payload.assign(buf_.begin() + off + HEADER, buf_.begin() + off + HEADER + plen);
            frames.push_back(std::move(f));
            off += total;
        } else {
            off += 1;                                          // bad CRC → resync
        }
    }
    buf_.erase(buf_.begin(), buf_.begin() + off);
    return frames;
}

std::vector<uint8_t> rleEncode(const uint8_t* px, size_t len) {
    std::vector<uint8_t> out;
    size_t i = 0;
    while (i < len) {
        uint8_t v = px[i] ? 1 : 0;
        size_t run = 1;
        while (i + run < len && (px[i + run] ? 1 : 0) == v && run < 255) run++;
        out.push_back((uint8_t)run);
        out.push_back(v);
        i += run;
    }
    return out;
}

std::vector<uint8_t> rleDecode(const uint8_t* data, size_t len) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < len; i += 2) {
        uint8_t run = data[i];
        uint8_t v = data[i + 1];
        for (uint8_t j = 0; j < run; j++) out.push_back(v);
    }
    return out;
}

} // namespace kairo::klp
