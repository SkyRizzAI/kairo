#include "nema/wallet/backends/nvs_backend.h"

#include <cstdlib>
#include <cstring>

extern "C" {
#include "bip39.h"
#include "pbkdf2.h"
#include "memzero.h"
#include "rand.h"
#include "aes/aes.h"
}

namespace nema::wallet {

namespace {

// Encrypted-seed blob layout:  [salt:16][iv:16][ciphertext:80]
//   key        = PBKDF2-HMAC-SHA256(pin, salt, ITER, 32 bytes)
//   plaintext  = [MAGIC:16][seed:64]   (80 bytes = 5 AES blocks)
//   ciphertext = AES-256-CBC(key, iv, plaintext)
// On unlock the MAGIC prefix is checked after decryption — a wrong PIN yields a
// wrong key → garbage plaintext → MAGIC mismatch → reject (no hash of the seed is
// stored). PBKDF2 iterations slow down offline PIN brute-force (the dev-mode /
// no-Secure-Boot threat noted in ADR 0014).
constexpr size_t kSalt = 16, kIv = 16, kSeed = 64, kMagic = 16;
constexpr size_t kPlain = kMagic + kSeed;            // 80
constexpr size_t kBlob = kSalt + kIv + kPlain;       // 112
constexpr uint32_t kIter = 10000;
constexpr uint8_t kMagicBytes[kMagic] = {
    'P', 'a', 'l', 'a', 'n', 'u', 'W', 'a', 'l', 'l', 'e', 't', 'v', '1', 0, 0};

void deriveKey(const std::string& pin, const uint8_t salt[kSalt], uint8_t key[32]) {
    pbkdf2_hmac_sha256(reinterpret_cast<const uint8_t*>(pin.data()), static_cast<int>(pin.size()),
                       salt, kSalt, kIter, key, 32);
}

}  // namespace

bool NvsBackend::create(const std::string& mnemonic, const std::string& passphrase,
                        const std::string& pin) {
    if (!HdWallet::validateMnemonic(mnemonic) || pin.empty()) return false;

    uint8_t seed[kSeed];
    mnemonic_to_seed(mnemonic.c_str(), passphrase.c_str(), seed, nullptr);

    // Assemble plaintext = MAGIC || seed.
    uint8_t plain[kPlain];
    std::memcpy(plain, kMagicBytes, kMagic);
    std::memcpy(plain + kMagic, seed, kSeed);

    // Random salt + iv.
    uint8_t blob[kBlob];
    random_buffer(blob, kSalt);            // salt
    random_buffer(blob + kSalt, kIv);      // iv

    uint8_t key[32];
    deriveKey(pin, blob, key);

    uint8_t ivWork[kIv];
    std::memcpy(ivWork, blob + kSalt, kIv);  // aes_cbc_encrypt mutates the iv in place
    aes_encrypt_ctx ctx;
    bool ok = aes_encrypt_key256(key, &ctx) == EXIT_SUCCESS &&
              aes_cbc_encrypt(plain, blob + kSalt + kIv, kPlain, ivWork, &ctx) == EXIT_SUCCESS;

    if (ok) ok = store_.save(blob, kBlob);
    if (ok) ok = hd_.unlockFromSeed(seed, kSeed);

    memzero(seed, sizeof(seed));
    memzero(plain, sizeof(plain));
    memzero(key, sizeof(key));
    memzero(ivWork, sizeof(ivWork));
    memzero(&ctx, sizeof(ctx));
    memzero(blob, sizeof(blob));
    return ok;
}

bool NvsBackend::unlock(const std::string& pin) {
    if (pin.empty()) return false;
    std::vector<uint8_t> blob;
    if (!store_.load(blob) || blob.size() != kBlob) return false;

    uint8_t key[32];
    deriveKey(pin, blob.data(), key);

    uint8_t ivWork[kIv];
    std::memcpy(ivWork, blob.data() + kSalt, kIv);
    uint8_t plain[kPlain];
    aes_decrypt_ctx ctx;
    bool ok = aes_decrypt_key256(key, &ctx) == EXIT_SUCCESS &&
              aes_cbc_decrypt(blob.data() + kSalt + kIv, plain, kPlain, ivWork, &ctx) == EXIT_SUCCESS;

    // Wrong PIN → wrong key → MAGIC won't match.
    if (ok) ok = std::memcmp(plain, kMagicBytes, kMagic) == 0;
    if (ok) ok = hd_.unlockFromSeed(plain + kMagic, kSeed);

    memzero(key, sizeof(key));
    memzero(ivWork, sizeof(ivWork));
    memzero(plain, sizeof(plain));
    memzero(&ctx, sizeof(ctx));
    return ok;
}

}  // namespace nema::wallet
