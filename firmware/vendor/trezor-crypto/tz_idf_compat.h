#pragma once
// ESP-IDF symbol-collision shim (Plan 94). trezor-crypto exports generic crypto
// names — hmac_sha256/hmac_sha512, aes_encrypt/aes_decrypt — that also exist in
// ESP-IDF's wpa_supplicant (crypto_mbedtls.c), producing "multiple definition"
// link errors. This header is force-included (-include) into EVERY trezor TU on the
// IDF build only, so both the definitions and trezor's internal callers are renamed
// consistently to a tz_ prefix. Our wallet code never calls these directly (it uses
// aes_cbc_encrypt / pbkdf2_hmac_sha256 / hdnode_*), so nothing external breaks.
#define hmac_sha256 tz_hmac_sha256
#define hmac_sha512 tz_hmac_sha512
#define aes_encrypt tz_aes_encrypt
#define aes_decrypt tz_aes_decrypt
