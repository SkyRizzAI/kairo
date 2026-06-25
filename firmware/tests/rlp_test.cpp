// Host unit test — RLP codec (Plan 94, Fase 2) against canonical RLP spec vectors.

#include "nema/wallet/chains/rlp.h"

#include <cstdio>
#include <string>

using namespace nema::wallet;

static int g_fails = 0;

static std::string hex(const Bytes& b) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (uint8_t x : b) { s += d[x >> 4]; s += d[x & 0xf]; }
    return s;
}
static Bytes str(const char* s) { return Bytes(s, s + std::char_traits<char>::length(s)); }

static void eq(const char* name, const std::string& got, const std::string& exp) {
    bool ok = got == exp;
    if (!ok) g_fails++;
    std::printf("[%s] %s = %s\n", ok ? "PASS" : "FAIL", name, got.c_str());
    if (!ok) std::printf("        expected %s\n", exp.c_str());
}

int main() {
    eq("dog",        hex(rlp::encodeString(str("dog"))),       "83646f67");
    eq("empty str",  hex(rlp::encodeString(Bytes{})),          "80");
    eq("byte 0x00",  hex(rlp::encodeString(Bytes{0x00})),      "00");
    eq("byte 0x0f",  hex(rlp::encodeString(Bytes{0x0f})),      "0f");
    eq("1024",       hex(rlp::encodeString(Bytes{0x04, 0x00})), "820400");
    eq("empty list", hex(rlp::encodeList({})),                 "c0");
    eq("[cat,dog]",
       hex(rlp::encodeList({rlp::encodeString(str("cat")), rlp::encodeString(str("dog"))})),
       "c88363617483646f67");

    // long string (>55 bytes) → 0xb8 + len
    {
        Bytes big(56, 0x61);  // 56 × 'a'
        Bytes enc = rlp::encodeString(big);
        bool ok = enc.size() == 58 && enc[0] == 0xb8 && enc[1] == 56;
        if (!ok) g_fails++;
        std::printf("[%s] long string header\n", ok ? "PASS" : "FAIL");
    }

    // decode round-trip
    {
        Bytes list = rlp::encodeList(
            {rlp::encodeString(str("cat")), rlp::encodeString(str("dog")), rlp::encodeString(Bytes{0x04, 0x00})});
        std::vector<Bytes> items;
        bool ok = rlp::decodeList(list, items) && items.size() == 3 &&
                  hex(items[0]) == "636174" && hex(items[1]) == "646f67" && hex(items[2]) == "0400";
        if (!ok) g_fails++;
        std::printf("[%s] decode round-trip (%zu items)\n", ok ? "PASS" : "FAIL", items.size());
    }

    std::printf("\n%s (%d failure%s)\n", g_fails ? "RLP TESTS FAILED" : "ALL RLP TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
