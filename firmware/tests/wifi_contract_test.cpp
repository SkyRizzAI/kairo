// Host contract test for IWifiDriver (Plan 72). Runs the WiFi state machine
// against the minimal NullWifiDriver and verifies the transitions + defaults a
// conforming driver must honour. A regression in any driver's state handling
// trips this.
#include "null_wifi_driver.h"
#include <cstdio>
#include <string>

using namespace nema;

static int fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); fail++; } \
                         else       std::printf("  ok:   %s\n", m); } while (0)

int main() {
    std::printf("== IWifiDriver contract (NullWifiDriver) ==\n");
    NullWifiDriver d;

    // initial state
    CHECK(d.state() == WifiState::Idle,        "starts Idle");
    CHECK(!d.isConnected(),                     "starts disconnected");
    CHECK(d.lastError() == WifiError::None,     "no error initially");
    CHECK(d.isEnabled(),                        "isEnabled default true (state != Disabled)");
    CHECK(d.savedCount() == 0,                  "no saved networks by default");

    // scan
    d.scan();
    CHECK(d.scanResults().size() == 1,          "scan returns the one AP");

    // successful connect → Connected
    CHECK(d.connect("TestAP"),                  "connect to known AP returns true");
    CHECK(d.state() == WifiState::Connected,    "state Connected after connect");
    CHECK(d.isConnected(),                       "isConnected true");
    CHECK(d.isOnline(),                          "isOnline default mirrors isConnected");
    CHECK(d.rssi() < 0,                          "rssi negative when connected");
    CHECK(std::string(d.ssid()) == "TestAP",    "ssid reflects the connected AP");

    // failed connect → Failed + error
    CHECK(!d.connect("Nope"),                    "connect to unknown AP returns false");
    CHECK(d.state() == WifiState::Failed,       "state Failed after bad connect");
    CHECK(d.lastError() == WifiError::ApNotFound,"lastError ApNotFound");

    // disconnect → Idle
    d.disconnect();
    CHECK(d.state() == WifiState::Idle,         "state Idle after disconnect");
    CHECK(!d.isConnected(),                      "disconnected");
    CHECK(d.rssi() == 0,                         "rssi 0 when not connected");

    // ip config round-trips
    WifiIpConfig c; c.dhcp = false;
    std::snprintf(c.ip, sizeof(c.ip), "192.168.9.9");
    d.setIpConfig(c);
    CHECK(!d.ipConfig().dhcp,                    "ipConfig persists dhcp flag");
    CHECK(std::string(d.ipConfig().ip) == "192.168.9.9", "ipConfig persists static ip");

    std::printf(fail ? "\nFAILED (%d)\n" : "\nPASS\n", fail);
    return fail ? 1 : 0;
}
