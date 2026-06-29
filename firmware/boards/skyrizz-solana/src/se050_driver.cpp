#include "nema/skyrizzsolana/se050_driver.h"
#include "nema/skyrizzsolana/board_config.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include <Wire.h>
#include <Arduino.h>
#include <esp_random.h>
#include <cstring>

// Real SE050 driver via the vendored NXP nano-package (component `se05x`). Mode-B
// seed sealing: a persistent in-chip AES-256 key wrap/unwraps the wallet's seed
// blob via AES-CBC, in a plain session (no SCP03). hasFeature(SecureStore) is
// gated by a boot-time wrap→unwrap self-test, so a non-working SE never makes the
// wallet trust it. Identical to the E32 driver except the chip is enabled via the
// direct SE_EN GPIO (no expander reset line on this board).

extern "C" {
#include "se05x_APDU_apis.h"
}

namespace nema::skyrizzsolana {

namespace {
Se05xSession_t g_session;
constexpr size_t kBlk = 16;   // AES block / IV size

std::string hx(const uint8_t* d, size_t n) {
    static const char* k = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n && i < 32; i++) { s += k[d[i] >> 4]; s += k[d[i] & 0xf]; }
    return s;
}
}  // namespace

bool Se050Driver::openSession() {
    // Enable the chip via direct SE_EN (GPIO8, HIGH=on) and let it boot.
    pinMode(PIN_SE_EN, OUTPUT);
    digitalWrite(PIN_SE_EN, HIGH);
    delay(10);

    std::memset(&g_session, 0, sizeof(g_session));
    g_session.skip_applet_select = 0;   // SessionOpen selects the IoT applet (plain session)

    smStatus_t st = Se05x_API_SessionOpen(&g_session);
    if (st != SM_OK) {
        rt_->log().warn("SE050", "nano SessionOpen failed", {{"sw", hx((uint8_t*)&st, 2)}});
        return false;
    }
    uint8_t ver[32]; size_t verLen = sizeof(ver);
    if (Se05x_API_GetVersion(&g_session, ver, &verLen) == SM_OK)
        rt_->log().info("SE050", "session open (nano-package)", {{"appletVer", hx(ver, verLen)}});
    else
        rt_->log().info("SE050", "session open (nano-package)");
    sessionOpen_ = true;
    return true;
}

bool Se050Driver::ensureAesKey() {
    SE05x_Result_t exists = kSE05x_Result_NA;
    if (Se05x_API_CheckObjectExists(&g_session, aesObjId_, &exists) == SM_OK &&
        exists == kSE05x_Result_SUCCESS) {
        rt_->log().info("SE050", "seal key exists");
        return true;
    }
    uint8_t key[32];
    esp_fill_random(key, sizeof(key));
    smStatus_t st = Se05x_API_WriteSymmKey(&g_session, NULL, /*maxAttempt*/ 0, aesObjId_,
                                           SE05x_KeyID_KEK_NONE, key, sizeof(key),
                                           kSE05x_INS_NA, kSE05x_SymmKeyType_AES);
    std::memset(key, 0, sizeof(key));
    rt_->log().info("SE050", "create seal key", {{"ok", st == SM_OK ? "1" : "0"}});
    return st == SM_OK;
}

bool Se050Driver::cipher(bool enc, const uint8_t* iv, const uint8_t* in, size_t n,
                         std::vector<uint8_t>& out) {
    out.resize(n + kBlk);
    size_t outLen = out.size();
    uint8_t ivbuf[kBlk];
    std::memcpy(ivbuf, iv, kBlk);
    smStatus_t st = Se05x_API_CipherOneShot(
        &g_session, aesObjId_, kSE05x_CipherMode_AES_CBC_NOPAD, in, n, ivbuf, kBlk,
        out.data(), &outLen,
        enc ? kSE05x_Cipher_Oper_OneShot_Encrypt : kSE05x_Cipher_Oper_OneShot_Decrypt);
    if (st != SM_OK) return false;
    out.resize(outLen);
    return true;
}

bool Se050Driver::randomBytes(uint8_t* out, size_t n) {
    if (!out) return false;
    esp_fill_random(out, n);          // ESP32 HW TRNG (nano-package exposes no GetRandom)
    return true;
}

bool Se050Driver::uniqueId(std::string& /*out*/) const {
    return false;                     // not needed for mode-B sealing
}

bool Se050Driver::wrap(uint8_t, const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    if (!secureStore_ || n == 0 || n % kBlk != 0) return false;
    uint8_t iv[kBlk];
    esp_fill_random(iv, sizeof(iv));
    std::vector<uint8_t> ct;
    if (!cipher(true, iv, in, n, ct)) return false;
    out.assign(iv, iv + kBlk);
    out.insert(out.end(), ct.begin(), ct.end());
    return true;
}

bool Se050Driver::unwrap(uint8_t, const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    if (!secureStore_ || n < 2 * kBlk || (n - kBlk) % kBlk != 0) return false;
    return cipher(false, in, in + kBlk, n - kBlk, out);
}

bool Se050Driver::selfTestSeal() {
    uint8_t plain[32];
    for (int i = 0; i < 32; i++) plain[i] = (uint8_t)(i * 7 + 1);
    secureStore_ = true;              // enable wrap/unwrap for the test only
    std::vector<uint8_t> sealed, back;
    bool ok = wrap(0, plain, sizeof(plain), sealed) &&
              unwrap(0, sealed.data(), sealed.size(), back) &&
              back.size() == sizeof(plain) && std::memcmp(back.data(), plain, sizeof(plain)) == 0;
    secureStore_ = ok;                // keep on ONLY if the round-trip matched
    return ok;
}

void Se050Driver::deinit() {
    if (sessionOpen_) { Se05x_API_SessionClose(&g_session); sessionOpen_ = false; }
}

void Se050Driver::init(Runtime& rt) {
    rt_ = &rt;

    // The SE050 talks via the SAME i2c_master bus as Wire but through OUR device
    // handle (scl_wait_us tolerant of its clock-stretch) — see se05x/port/sm_i2c.cpp.
    // No Wire presence probe (it would add 0x48 with scl_wait_us=0 and block our add).
    // The self-test gates SecureStore (fail-closed): any failure → wallet software.
    constexpr bool kEnableBringup = true;
    if (!kEnableBringup) {
        rt.log().info("SE050", "bring-up gated off — wallet stays software");
        return;
    }

    bool sess = openSession();
    present_ = sess;
    bool key  = sess && ensureAesKey();
    bool seal = key && selfTestSeal();
    secureStore_ = seal;
    rt.log().info("SE050", "bring-up (nano-package)",
                  {{"session", sess ? "1" : "0"}, {"sealKey", key ? "1" : "0"},
                   {"selfTest", seal ? "1" : "0"},
                   {"secureStore", secureStore_ ? "ENABLED (wallet -> mode B)"
                                                : "disabled (wallet -> software)"}});
}

} // namespace nema::skyrizzsolana
