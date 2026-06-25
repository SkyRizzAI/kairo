// Platform RNG hooks for trezor-crypto (Plan 94). trezor-crypto declares but does
// not define random32()/random_buffer() — the platform supplies them. Under
// Emscripten, getentropy() is backed by the browser CSPRNG (crypto.getRandomValues).
#include <cstddef>
#include <cstdint>
#include <sys/random.h>  // getentropy

extern "C" uint32_t random32(void) {
    uint32_t v = 0;
    getentropy(&v, sizeof(v));
    return v;
}

extern "C" void random_buffer(uint8_t* buf, size_t len) {
    while (len) {
        size_t n = len > 256 ? 256 : len;  // getentropy caps at 256 bytes/call
        getentropy(buf, n);
        buf += n;
        len -= n;
    }
}
