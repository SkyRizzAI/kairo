#include "nema/wallet/backends/se_backend.h"

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

// PIN layer (same as NvsBackend) wrapped by the SE:
//   plaintext = [MAGIC:16][seed:64]                              (80 bytes)
//   blob1     = [salt:16][iv:16][AES-256-CBC(pinKey, iv, plaintext):80]  (112 bytes)
//   stored    = SE.wrap(blob1)                                   (device-bound)
// Recovery needs the PIN (→ MAGIC check) AND the chip (→ unwrap). A wrong PIN or a
// different device both fail the MAGIC check.
constexpr size_t kSalt = 16, kIv = 16, kSeed = 64, kMagic = 16;
constexpr size_t kPlain = kMagic + kSeed;        // 80
constexpr size_t kBlob1 = kSalt + kIv + kPlain;  // 112
constexpr uint32_t kIter = 10000;
constexpr uint8_t kMagicBytes[kMagic] = {
    'P', 'a', 'l', 'a', 'n', 'u', 'S', 'E', 'm', 'o', 'd', 'e', 'B', '1', 0, 0};

void deriveKey(const std::string& pin, const uint8_t salt[kSalt], uint8_t key[32]) {
    pbkdf2_hmac_sha256(reinterpret_cast<const uint8_t*>(pin.data()), static_cast<int>(pin.size()),
                       salt, kSalt, kIter, key, 32);
}

}  // namespace

bool SeBackend::create(const std::string& mnemonic, const std::string& passphrase,
                       const std::string& pin) {
    if (!HdWallet::validateMnemonic(mnemonic) || pin.empty() || !supportedBy(se_)) return false;

    uint8_t seed[kSeed];
    mnemonic_to_seed(mnemonic.c_str(), passphrase.c_str(), seed, nullptr);

    uint8_t blob1[kBlob1];
    random_buffer(blob1, kSalt);             // salt
    random_buffer(blob1 + kSalt, kIv);       // iv

    uint8_t plain[kPlain];
    std::memcpy(plain, kMagicBytes, kMagic);
    std::memcpy(plain + kMagic, seed, kSeed);

    uint8_t key[32];
    deriveKey(pin, blob1, key);
    uint8_t ivWork[kIv];
    std::memcpy(ivWork, blob1 + kSalt, kIv);
    aes_encrypt_ctx ctx;
    bool ok = aes_encrypt_key256(key, &ctx) == EXIT_SUCCESS &&
              aes_cbc_encrypt(plain, blob1 + kSalt + kIv, kPlain, ivWork, &ctx) == EXIT_SUCCESS;

    std::vector<uint8_t> wrapped;
    if (ok) ok = se_.wrap(slot_, blob1, kBlob1, wrapped);     // device-bound seal
    if (ok) ok = store_.save(wrapped.data(), wrapped.size());
    if (ok) ok = hd_.unlockFromSeed(seed, kSeed);

    memzero(seed, sizeof(seed));
    memzero(plain, sizeof(plain));
    memzero(key, sizeof(key));
    memzero(ivWork, sizeof(ivWork));
    memzero(&ctx, sizeof(ctx));
    memzero(blob1, sizeof(blob1));
    return ok;
}

bool SeBackend::unlock(const std::string& pin) {
    if (pin.empty()) return false;
    std::vector<uint8_t> wrapped;
    if (!store_.load(wrapped) || wrapped.empty()) return false;

    std::vector<uint8_t> blob1;
    if (!se_.unwrap(slot_, wrapped.data(), wrapped.size(), blob1) || blob1.size() != kBlob1)
        return false;  // wrong device → unwrap fails or wrong length

    uint8_t key[32];
    deriveKey(pin, blob1.data(), key);
    uint8_t ivWork[kIv];
    std::memcpy(ivWork, blob1.data() + kSalt, kIv);
    uint8_t plain[kPlain];
    aes_decrypt_ctx ctx;
    bool ok = aes_decrypt_key256(key, &ctx) == EXIT_SUCCESS &&
              aes_cbc_decrypt(blob1.data() + kSalt + kIv, plain, kPlain, ivWork, &ctx) == EXIT_SUCCESS;

    if (ok) ok = std::memcmp(plain, kMagicBytes, kMagic) == 0;  // wrong PIN → mismatch
    if (ok) ok = hd_.unlockFromSeed(plain + kMagic, kSeed);

    memzero(key, sizeof(key));
    memzero(ivWork, sizeof(ivWork));
    memzero(plain, sizeof(plain));
    memzero(&ctx, sizeof(ctx));
    if (!blob1.empty()) memzero(blob1.data(), blob1.size());
    return ok;
}

}  // namespace nema::wallet
