// Host unit test for the KLP codec (Plan 35). Mirrors the TS test vectors in
// packages/forge/src/lib/klp/codec.test.ts so both codecs stay byte-exact.
#include "kairo/link/klp_codec.h"
#include "kairo/link/transport.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace kairo;
using namespace kairo::klp;

static std::vector<uint8_t> enc(uint8_t ch, std::vector<uint8_t> p, uint8_t fl = 0) {
    std::vector<uint8_t> out;
    encodeFrame(out, ch, p.data(), p.size(), fl);
    return out;
}

int main() {
    // 1. encode → parse roundtrip
    {
        auto bytes = enc((uint8_t)Channel::Input, {1, 2, 3, 4, 5});
        FrameParser p;
        auto frames = p.push(bytes.data(), bytes.size());
        assert(frames.size() == 1);
        assert(frames[0].channel == (uint8_t)Channel::Input);
        assert((frames[0].payload == std::vector<uint8_t>{1, 2, 3, 4, 5}));
    }

    // 2. reassemble across byte-by-byte chunks
    {
        auto f = enc((uint8_t)Channel::Screen, {9, 8, 7}, Flags::Compressed);
        FrameParser p;
        std::vector<Frame> got;
        for (uint8_t b : f) { auto r = p.push(&b, 1); for (auto& x : r) got.push_back(x); }
        assert(got.size() == 1);
        assert(got[0].flags & Flags::Compressed);
        assert((got[0].payload == std::vector<uint8_t>{9, 8, 7}));
    }

    // 3. two frames in one chunk
    {
        auto a = enc((uint8_t)Channel::Log, {1});
        auto b = enc((uint8_t)Channel::System, {2, 2});
        std::vector<uint8_t> merged = a;
        merged.insert(merged.end(), b.begin(), b.end());
        FrameParser p;
        auto frames = p.push(merged.data(), merged.size());
        assert(frames.size() == 2);
        assert(frames[0].channel == (uint8_t)Channel::Log);
        assert(frames[1].channel == (uint8_t)Channel::System);
    }

    // 4. CRC corruption rejected then resync to the good frame
    {
        auto good = enc((uint8_t)Channel::Control, {5, 5, 5});
        good.back() ^= 0xff; // wreck CRC
        auto next = enc((uint8_t)Channel::Input, {7});
        std::vector<uint8_t> merged = good;
        merged.insert(merged.end(), next.begin(), next.end());
        FrameParser p;
        auto frames = p.push(merged.data(), merged.size());
        bool sawInput = false;
        for (auto& f : frames) if (f.channel == (uint8_t)Channel::Input) sawInput = true;
        assert(sawInput);
    }

    // 5. crc8 determinism + known value
    {
        uint8_t z = 0x00;
        assert(crc8(&z, 1) == 0x00);
    }

    // 6. RLE roundtrip on a 1-bit framebuffer
    {
        const int w = 64, h = 8;
        std::vector<uint8_t> px(w * h);
        for (size_t i = 0; i < px.size(); i++) px[i] = (i % 17 < 3) ? 1 : 0;
        auto e = rleEncode(px.data(), px.size());
        auto d = rleDecode(e.data(), e.size());
        assert(d == px);
        assert(e.size() < px.size()); // actually compresses
    }

    // 7. virtual cable (loopback) carries KLP both ways
    {
        LoopbackTransport a, b;
        a.setPeer(&b);
        b.setPeer(&a);
        static FrameParser pa, pb;
        static std::vector<uint8_t> recvA, recvB;
        recvA.clear(); recvB.clear();
        a.onRecv([](void*, const uint8_t* d, size_t n) {
            for (auto& f : pa.push(d, n)) recvA.push_back(f.channel);
        }, nullptr);
        b.onRecv([](void*, const uint8_t* d, size_t n) {
            for (auto& f : pb.push(d, n)) recvB.push_back(f.channel);
        }, nullptr);

        const uint8_t shot[4] = {0, 0, 1, 1};
        auto screen = rleEncode(shot, 4);
        auto sf = enc((uint8_t)Channel::Screen, screen, Flags::Compressed);
        a.send(sf.data(), sf.size());                 // device → remote
        auto inf = enc((uint8_t)Channel::Input, {2});
        b.send(inf.data(), inf.size());               // remote → device

        bool gotScreen = false, gotInput = false;
        for (auto c : recvB) if (c == (uint8_t)Channel::Screen) gotScreen = true;
        for (auto c : recvA) if (c == (uint8_t)Channel::Input) gotInput = true;
        assert(gotScreen);
        assert(gotInput);
    }

    std::printf("klp_test OK\n");
    return 0;
}
