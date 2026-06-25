#include "nema/wallet/wallet_vault.h"

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
constexpr size_t kSalt = 16, kIv = 16, kSeed = 64, kMagic = 16;
constexpr size_t kPlain = kMagic + kSeed;        // 80
constexpr size_t kBlob = kSalt + kIv + kPlain;   // 112
constexpr uint32_t kIter = 10000;
constexpr uint8_t kMagicBytes[kMagic] = {
    'P', 'a', 'l', 'a', 'n', 'u', 'V', 'a', 'u', 'l', 't', 'v', '1', 0, 0, 0};
const char* kIndexKey = "vault.idx";

void deriveKey(const std::string& pin, const uint8_t salt[kSalt], uint8_t key[32]) {
    pbkdf2_hmac_sha256(reinterpret_cast<const uint8_t*>(pin.data()), static_cast<int>(pin.size()),
                       salt, kSalt, kIter, key, 32);
}
}  // namespace

WalletVault::WalletVault(IKvStore& store, ISecureElement* se, uint8_t seSlot)
    : store_(store), se_(se), seSlot_(seSlot) { loadIndex(); }

// ── index (plaintext: activeId on line 0, then "id\tlabel" per wallet) ──
bool WalletVault::loadIndex() {
    metas_.clear();
    activeId_.clear();
    std::vector<uint8_t> raw;
    if (!store_.get(kIndexKey, raw) || raw.empty()) return false;
    std::string s(raw.begin(), raw.end());
    size_t pos = 0;
    bool first = true;
    while (pos < s.size()) {
        size_t nl = s.find('\n', pos);
        std::string line = s.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? s.size() : nl + 1;
        if (first) { activeId_ = line; first = false; continue; }
        if (line.empty()) continue;
        size_t t1 = line.find('\t');
        if (t1 == std::string::npos) continue;
        size_t t2 = line.find('\t', t1 + 1);
        WalletMeta wm;
        wm.id = line.substr(0, t1);
        if (t2 == std::string::npos) {
            wm.label = line.substr(t1 + 1);
            wm.accounts = 1;
        } else {
            wm.label = line.substr(t1 + 1, t2 - t1 - 1);
            unsigned long n = std::strtoul(line.c_str() + t2 + 1, nullptr, 10);
            wm.accounts = n < 1 ? 1 : static_cast<uint32_t>(n);
        }
        metas_.push_back(wm);
    }
    return true;
}

bool WalletVault::saveIndex() {
    std::string s = activeId_ + "\n";
    for (const auto& m : metas_)
        s += m.id + "\t" + m.label + "\t" + std::to_string(m.accounts) + "\n";
    return store_.put(kIndexKey, reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

// ── seed cipher ──
// inner blob = salt||iv||AES-256-CBC(pinKey, iv, MAGIC||seed)  (mode C, always).
// mode B: the inner blob is additionally wrapped by the SE's device-bound key, so the
// stored bytes need BOTH the PIN and the physical chip. Software path is byte-identical
// to before (no SE → out == inner), so existing software wallets still decrypt.
bool WalletVault::encryptSeed(const uint8_t seed[64], const std::string& pin,
                              std::vector<uint8_t>& out) {
    uint8_t plain[kPlain];
    std::memcpy(plain, kMagicBytes, kMagic);
    std::memcpy(plain + kMagic, seed, kSeed);
    std::vector<uint8_t> inner(kBlob);
    random_buffer(inner.data(), kSalt);
    random_buffer(inner.data() + kSalt, kIv);
    uint8_t key[32];
    deriveKey(pin, inner.data(), key);
    uint8_t iv[kIv];
    std::memcpy(iv, inner.data() + kSalt, kIv);
    aes_encrypt_ctx ctx;
    bool ok = aes_encrypt_key256(key, &ctx) == EXIT_SUCCESS &&
              aes_cbc_encrypt(plain, inner.data() + kSalt + kIv, kPlain, iv, &ctx) == EXIT_SUCCESS;
    memzero(plain, sizeof(plain));
    memzero(key, sizeof(key));
    memzero(&ctx, sizeof(ctx));
    if (!ok) return false;

    if (se_) {  // mode B — device-bound seal
        std::vector<uint8_t> wrapped;
        ok = se_->wrap(seSlot_, inner.data(), inner.size(), wrapped);
        memzero(inner.data(), inner.size());
        if (!ok) return false;
        out = std::move(wrapped);
    } else {
        out = std::move(inner);
    }
    return true;
}

bool WalletVault::decryptSeed(const std::vector<uint8_t>& blob, const std::string& pin,
                              uint8_t seed[64]) {
    std::vector<uint8_t> inner;
    if (se_) {                                       // mode B — unseal first
        if (!se_->unwrap(seSlot_, blob.data(), blob.size(), inner)) return false;  // wrong chip
    } else {
        inner = blob;
    }
    if (inner.size() != kBlob) return false;
    uint8_t key[32];
    deriveKey(pin, inner.data(), key);
    uint8_t iv[kIv];
    std::memcpy(iv, inner.data() + kSalt, kIv);
    uint8_t plain[kPlain];
    aes_decrypt_ctx ctx;
    bool ok = aes_decrypt_key256(key, &ctx) == EXIT_SUCCESS &&
              aes_cbc_decrypt(inner.data() + kSalt + kIv, plain, kPlain, iv, &ctx) == EXIT_SUCCESS;
    if (ok) ok = std::memcmp(plain, kMagicBytes, kMagic) == 0;   // wrong PIN → mismatch
    if (ok) std::memcpy(seed, plain + kMagic, kSeed);
    memzero(key, sizeof(key));
    memzero(plain, sizeof(plain));
    memzero(&ctx, sizeof(ctx));
    if (!inner.empty()) memzero(inner.data(), inner.size());
    return ok;
}

std::string WalletVault::genId() {
    uint8_t r[6];
    random_buffer(r, sizeof(r));
    static const char* h = "0123456789abcdef";
    std::string id;
    for (uint8_t b : r) { id += h[b >> 4]; id += h[b & 0xf]; }
    return id;
}

bool WalletVault::createFirst(const std::string& mnemonic, const std::string& pin,
                              std::string& outId) {
    if (!metas_.empty() || pin.empty() || !HdWallet::validateMnemonic(mnemonic)) return false;
    uint8_t seed[kSeed];
    mnemonic_to_seed(mnemonic.c_str(), "", seed, nullptr);
    std::vector<uint8_t> blob;
    bool ok = encryptSeed(seed, pin, blob);
    std::string id = genId();
    if (ok) ok = store_.put(walletKey(id).c_str(), blob.data(), blob.size());
    if (ok) {
        metas_.push_back({id, "Wallet 1"});
        activeId_ = id;
        pin_ = pin;
        unlocked_ = true;
        ok = hd_.unlockFromSeed(seed, kSeed) && saveIndex();
        outId = id;
    }
    memzero(seed, sizeof(seed));
    return ok;
}

bool WalletVault::addWallet(const std::string& mnemonic, std::string& outId) {
    if (!unlocked_ || !HdWallet::validateMnemonic(mnemonic)) return false;
    uint8_t seed[kSeed];
    mnemonic_to_seed(mnemonic.c_str(), "", seed, nullptr);
    std::vector<uint8_t> blob;
    bool ok = encryptSeed(seed, pin_, blob);
    std::string id = genId();
    if (ok) ok = store_.put(walletKey(id).c_str(), blob.data(), blob.size());
    if (ok) {
        metas_.push_back({id, "Wallet " + std::to_string(metas_.size() + 1)});
        activeId_ = id;
        ok = hd_.unlockFromSeed(seed, kSeed) && saveIndex();
        outId = id;
    }
    memzero(seed, sizeof(seed));
    return ok;
}

bool WalletVault::unlock(const std::string& pin) {
    if (pin.empty() || metas_.empty()) return false;
    const std::string& id = activeId_.empty() ? metas_.front().id : activeId_;
    std::vector<uint8_t> blob;
    if (!store_.get(walletKey(id).c_str(), blob)) return false;
    uint8_t seed[kSeed];
    if (!decryptSeed(blob, pin, seed)) return false;   // wrong PIN
    pin_ = pin;
    unlocked_ = true;
    activeId_ = id;
    bool ok = hd_.unlockFromSeed(seed, kSeed);
    memzero(seed, sizeof(seed));
    return ok;
}

bool WalletVault::select(const std::string& id) {
    if (!unlocked_) return false;
    bool known = false;
    for (const auto& m : metas_) if (m.id == id) { known = true; break; }
    if (!known) return false;
    std::vector<uint8_t> blob;
    if (!store_.get(walletKey(id).c_str(), blob)) return false;
    uint8_t seed[kSeed];
    if (!decryptSeed(blob, pin_, seed)) return false;
    bool ok = hd_.unlockFromSeed(seed, kSeed);
    if (ok) { activeId_ = id; saveIndex(); }
    memzero(seed, sizeof(seed));
    return ok;
}

bool WalletVault::remove(const std::string& id) {
    bool found = false;
    for (size_t i = 0; i < metas_.size(); i++)
        if (metas_[i].id == id) { metas_.erase(metas_.begin() + i); found = true; break; }
    if (!found) return false;
    store_.del(walletKey(id).c_str());
    if (activeId_ == id) {
        activeId_.clear();
        hd_.lock();
        if (!metas_.empty() && unlocked_) select(metas_.front().id);
    }
    return saveIndex();
}

uint32_t WalletVault::activeAccountCount() const {
    for (const auto& m : metas_) if (m.id == activeId_) return m.accounts;
    return 1;
}

bool WalletVault::addAccount() {
    if (!unlocked_) return false;
    for (auto& m : metas_)
        if (m.id == activeId_) { m.accounts++; return saveIndex(); }
    return false;
}

void WalletVault::lock() {
    memzero(pin_.empty() ? nullptr : &pin_[0], pin_.size());
    pin_.clear();
    unlocked_ = false;
    hd_.lock();
}

void WalletVault::wipe() {
    for (const auto& m : metas_) store_.del(walletKey(m.id).c_str());
    store_.del(kIndexKey);
    metas_.clear();
    activeId_.clear();
    lock();
}

}  // namespace nema::wallet
