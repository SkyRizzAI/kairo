#include "nema/wallet/soft_secure_element.h"

#include <cstring>

extern "C" {
#include "rand.h"
#include "aes/aes.h"
}

namespace nema {

SoftSecureElement::SoftSecureElement() {
    // Deterministic default device key (sim/host). Real devices have a unique chip key.
    for (int i = 0; i < 32; i++) key_[i] = static_cast<uint8_t>(0xA0 + i);
}

SoftSecureElement::SoftSecureElement(const uint8_t deviceKey[32]) {
    std::memcpy(key_, deviceKey, 32);
}

bool SoftSecureElement::randomBytes(uint8_t* out, size_t n) {
    random_buffer(out, n);
    return true;
}

bool SoftSecureElement::uniqueId(std::string& out) const {
    static const char* kHex = "0123456789abcdef";
    out.clear();
    for (int i = 0; i < 8; i++) { out += kHex[key_[i] >> 4]; out += kHex[key_[i] & 0xf]; }
    return true;
}

// wrap: out = iv(16) || AES-256-CBC(key, iv, in). `in` length must be a multiple of 16.
bool SoftSecureElement::wrap(uint8_t, const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    if (n == 0 || n % 16 != 0) return false;
    out.resize(16 + n);
    random_buffer(out.data(), 16);  // iv
    uint8_t iv[16];
    std::memcpy(iv, out.data(), 16);
    aes_encrypt_ctx ctx;
    bool ok = aes_encrypt_key256(key_, &ctx) == EXIT_SUCCESS &&
              aes_cbc_encrypt(in, out.data() + 16, static_cast<int>(n), iv, &ctx) == EXIT_SUCCESS;
    std::memset(&ctx, 0, sizeof(ctx));
    return ok;
}

// unwrap: in = iv(16) || ciphertext. Wrong device key → garbage (caller's MAGIC check rejects).
bool SoftSecureElement::unwrap(uint8_t, const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    if (n < 32 || (n - 16) % 16 != 0) return false;
    uint8_t iv[16];
    std::memcpy(iv, in, 16);
    out.resize(n - 16);
    aes_decrypt_ctx ctx;
    bool ok = aes_decrypt_key256(key_, &ctx) == EXIT_SUCCESS &&
              aes_cbc_decrypt(in + 16, out.data(), static_cast<int>(n - 16), iv, &ctx) == EXIT_SUCCESS;
    std::memset(&ctx, 0, sizeof(ctx));
    return ok;
}

}  // namespace nema
