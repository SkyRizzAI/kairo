// SHA-256 — portable, public domain (FIPS 180-4).
// No platform dependencies: compiles identically on host, WASM, and ESP32.
#include "nema/crypto/sha256.h"
#include <cstring>
#include <cstdint>

namespace nema {
namespace {

static inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32u - n));
}

static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static void processBlock(uint32_t h[8], const uint8_t blk[64]) {
    uint32_t W[64];
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)blk[i*4  ] << 24) |
               ((uint32_t)blk[i*4+1] << 16) |
               ((uint32_t)blk[i*4+2] <<  8) |
               ((uint32_t)blk[i*4+3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(W[i-15], 7) ^ rotr(W[i-15], 18) ^ (W[i-15] >> 3);
        uint32_t s1 = rotr(W[i-2], 17) ^ rotr(W[i-2], 19)  ^ (W[i-2] >> 10);
        W[i] = W[i-16] + s0 + W[i-7] + s1;
    }
    uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], v=h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1  = rotr(e,6)  ^ rotr(e,11) ^ rotr(e,25);
        uint32_t ch  = (e & f) ^ (~e & g);
        uint32_t T1  = v + S1 + ch + K[i] + W[i];
        uint32_t S0  = rotr(a,2)  ^ rotr(a,13) ^ rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t T2  = S0 + maj;
        v=g; g=f; f=e; e=d+T1; d=c; c=b; b=a; a=T1+T2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
    h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=v;
}

} // namespace

std::string hexSha256(const std::string& data) {
    uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };

    const uint8_t* msg = reinterpret_cast<const uint8_t*>(data.data());
    size_t   len    = data.size();
    uint64_t bitLen = (uint64_t)len * 8;

    // Full blocks
    size_t i = 0;
    for (; i + 64 <= len; i += 64)
        processBlock(h, msg + i);

    // Padding: append 0x80, zeros, then 64-bit big-endian bit-length
    uint8_t buf[128] = {};
    size_t rem = len - i;
    if (rem) memcpy(buf, msg + i, rem);
    buf[rem] = 0x80u;
    size_t nblocks = (rem < 56) ? 1 : 2;
    size_t end = nblocks * 64;
    for (int j = 0; j < 8; j++)
        buf[end - 8 + j] = (uint8_t)(bitLen >> (56 - j * 8));
    for (size_t b = 0; b < nblocks; b++)
        processBlock(h, buf + b * 64);

    // Encode to lowercase hex
    static const char HX[] = "0123456789abcdef";
    std::string out(64, '\0');
    for (int j = 0; j < 8; j++) {
        out[j*8  ] = HX[(h[j]>>28)&0xF]; out[j*8+1] = HX[(h[j]>>24)&0xF];
        out[j*8+2] = HX[(h[j]>>20)&0xF]; out[j*8+3] = HX[(h[j]>>16)&0xF];
        out[j*8+4] = HX[(h[j]>>12)&0xF]; out[j*8+5] = HX[(h[j]>> 8)&0xF];
        out[j*8+6] = HX[(h[j]>> 4)&0xF]; out[j*8+7] = HX[ h[j]      &0xF];
    }
    return out;
}

std::string randomHexSalt(size_t n) {
    // XorShift32 PRNG. Seed uses the static variable's address so it differs
    // per device binary; combined with a compile-time mix constant.
    static uint32_t state = 0;
    if (state == 0) {
        state = (uint32_t)(uintptr_t)&state ^ 0xDEAD1337u;
        if (state == 0) state = 0xABCD1234u;
    }
    static const char HX[] = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; i++) {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        out += HX[(state >> 4) & 0xFu];
        out += HX[ state       & 0xFu];
    }
    return out;
}

} // namespace nema
