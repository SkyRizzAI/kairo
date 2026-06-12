// Host test for the remote layer (Plan 35): LinkService handshake + channel
// routing + RemoteScreenTap streaming, all over a loopback "virtual cable".
#include "nema/link/link_service.h"
#include "nema/link/transport.h"
#include "nema/link/klp_codec.h"
#include "nema/hal/remote_screen_tap.h"
#include "nema/hal/display.h"
#include "nema/services/remote_service.h"
#include "nema/services/input_service.h"
#include "nema/log/logger.h"
#include "nema/clock.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace nema;

// Minimal inner display for the tap to wrap.
class DummyDisplay : public IDisplayDriver {
public:
    const char* name() const override { return "Dummy"; }
    DriverKind  kind() const override { return DriverKind::Display; }
    uint16_t width()  const override { return 16; }
    uint16_t height() const override { return 4; }
    void drawPixel(uint16_t, uint16_t, bool) override {}
    void fillRect(uint16_t, uint16_t, uint16_t, uint16_t, bool) override {}
    void clear(bool = false) override {}
    void flush() override {}
};

static std::vector<klp::Frame> g_hostFrames;
static std::vector<klp::Frame> g_devFrames;
static void hostSink(void*, const klp::Frame& f) { g_hostFrames.push_back(f); }
static void devSink(void*, const klp::Frame& f) { g_devFrames.push_back(f); }

int main() {
    LoopbackTransport devCable, hostCable;
    devCable.setPeer(&hostCable);
    hostCable.setPeer(&devCable);

    LinkService device, host;
    device.attach(&devCable, LinkService::Role::Device);
    host.attach(&hostCable, LinkService::Role::Host);
    device.onFrame(devSink, nullptr);
    host.onFrame(hostSink, nullptr);

    // Channels gated before handshake.
    {
        uint8_t x = 1;
        device.send(klp::Channel::Screen, &x, 1);
        assert(g_hostFrames.empty());   // dropped, not ready
    }

    // Handshake.
    host.begin();
    assert(device.ready());
    assert(host.ready());

    // Screen streaming via the tap.
    DummyDisplay dummy;
    RemoteScreenTap tap;
    tap.init(dummy, device);
    tap.clear(false);
    tap.drawPixel(2, 1, true);   // index = 1*16 + 2 = 18
    tap.flush();

    assert(!g_hostFrames.empty());
    const klp::Frame& sf = g_hostFrames.back();
    assert(sf.channel == (uint8_t)klp::Channel::Screen);
    assert(sf.flags & klp::Flags::Compressed);
    // decode payload: [w:2][h:2][rle...]
    uint16_t w = sf.payload[0] | (sf.payload[1] << 8);
    uint16_t h = sf.payload[2] | (sf.payload[3] << 8);
    assert(w == 16 && h == 4);
    auto px = klp::rleDecode(sf.payload.data() + 4, sf.payload.size() - 4);
    assert(px.size() == (size_t)w * h);
    assert(px[18] == 1);
    for (size_t i = 0; i < px.size(); i++) if (i != 18) assert(px[i] == 0);

    // Input from host → device.
    uint8_t code = 0x02;   // e.g. Down
    host.send(klp::Channel::Input, &code, 1);
    assert(!g_devFrames.empty());
    assert(g_devFrames.back().channel == (uint8_t)klp::Channel::Input);
    assert(g_devFrames.back().payload.size() == 1 && g_devFrames.back().payload[0] == 0x02);

    // RemoteService: INPUT → InputService, device logs → LOG channel.
    {
        struct TestClock : IClock {
            uint64_t millis() override { return 0; }
            uint64_t epochMs() override { return 0; }
        } clock;
        InputService input;
        RemoteService remote;
        remote.init(device, input);      // device link now routes via RemoteService
        Logger log(clock);
        remote.attachLog(log);

        uint8_t down = (uint8_t)Key::Down;
        host.send(klp::Channel::Input, &down, 1);
        InputEvent ev;
        assert(input.next(ev));
        assert(ev.key == Key::Down);

        size_t before = g_hostFrames.size();
        log.info("RemoteTest", "hello");
        bool sawLog = false;
        for (size_t i = before; i < g_hostFrames.size(); i++)
            if (g_hostFrames[i].channel == (uint8_t)klp::Channel::Log) sawLog = true;
        assert(sawLog);
    }

    std::printf("link_test OK\n");
    return 0;
}
