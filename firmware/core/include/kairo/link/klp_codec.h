#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

// Kairo Link Protocol (KLP) — wire codec (C++ device side).
//
// Byte-exact mirror of the TS codec (packages/forge/src/lib/klp/codec.ts).
// One protocol, many transports: the same frames travel over BLE, USB-CDC, or
// the simulator virtual-cable. See Plan 35.
//
// Frame: [magic:0xAB][chan:1][flags:1][len:2 LE][payload:len][crc8:1]
namespace kairo::klp {

enum class Channel : uint8_t {
    Control = 0x00,
    Screen  = 0x01,
    Input   = 0x02,
    Log     = 0x03,
    System  = 0x04,
    Ota     = 0x05,
    Ext     = 0x06,   // host→device sim-control commands (inject event, wifi router)
    Event   = 0x07,   // device→host EventBus stream (for the Events panel)
    Cli     = 0x08,   // bidirectional terminal: host sends a command line, device
                      // streams text output, ending with a single 0x04 (EOT) frame
    File    = 0x09,   // filesystem request/response (list/read/write/mkdir/remove)
};

namespace Flags {
    enum : uint8_t { None = 0, FragMore = 1 << 0, Compressed = 1 << 1 };
}

constexpr uint8_t MAGIC = 0xAB;

// CRC-8/SMBus (poly 0x07, init 0x00).
uint8_t crc8(const uint8_t* data, size_t len);

// Encode one frame. Appends to `out` (cleared first).
void encodeFrame(std::vector<uint8_t>& out, uint8_t channel,
                 const uint8_t* payload, size_t len, uint8_t flags = 0);

struct Frame {
    uint8_t              channel = 0;
    uint8_t              flags   = 0;
    std::vector<uint8_t> payload;
};

// Stream parser — feed arbitrary byte chunks, get back complete frames.
// Resyncs on the MAGIC sync byte after a CRC mismatch.
class FrameParser {
public:
    // Append bytes; return any frames now complete.
    std::vector<Frame> push(const uint8_t* data, size_t len);
    void reset() { buf_.clear(); }

private:
    std::vector<uint8_t> buf_;
};

// RLE for 1-bit framebuffers (w*h bytes, each 0/1): pairs of [count][value],
// runs > 255 split. Compresses the mostly-blank monochrome UI well.
std::vector<uint8_t> rleEncode(const uint8_t* px, size_t len);
std::vector<uint8_t> rleDecode(const uint8_t* data, size_t len);

} // namespace kairo::klp
