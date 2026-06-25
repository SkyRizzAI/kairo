# Vendored trezor-crypto (MIT) — wallet crypto core for Plan 94.
#
# Curated subset of trezor-firmware/crypto: secp256k1 + nist256p1 (ECDSA, RFC6979,
# low-S), ed25519 (EdDSA, SLIP-0010), BIP32/BIP39/BIP44, keccak/sha3, sha2, base58,
# bech32 (segwit_addr). See README.md for provenance + the curated file list.
#
# Dual-build: ESP-IDF component (firmware) and plain static lib (host tests/sim),
# mirroring firmware/vendor/quickjs. 3rd-party C: warnings disabled so it does not
# trip our -Wall/-Werror.
#
# ⚠️ trezor-crypto does NOT implement random32()/random_buffer() — the PLATFORM must
# provide them (firmware: SE050 TRNG / esp_random; host tests: a stub). Linking this
# library without supplying those two symbols is an intentional link error.

set(TZ_SRCS
    address.c base58.c bignum.c bip32.c bip39.c bip39_english.c
    blake256.c blake2b.c blake2s.c curves.c ecdsa.c groestl.c hasher.c
    hmac.c hmac_drbg.c memzero.c nist256p1.c pbkdf2.c rand.c rfc6979.c
    ripemd160.c secp256k1.c segwit_addr.c sha2.c sha3.c
    aes/aescrypt.c aes/aeskey.c aes/aestab.c aes/aes_modes.c
    ed25519-donna/ed25519.c
    ed25519-donna/curve25519-donna-32bit.c
    ed25519-donna/curve25519-donna-helpers.c
    ed25519-donna/curve25519-donna-scalarmult-base.c
    ed25519-donna/modm-donna-32bit.c
    ed25519-donna/ed25519-donna-32bit-tables.c
    ed25519-donna/ed25519-donna-basepoint-table.c
    ed25519-donna/ed25519-donna-impl-base.c
    ed25519-donna/ed25519-keccak.c
    ed25519-donna/ed25519-sha3.c
)

if(ESP_PLATFORM)
    idf_component_register(SRCS ${TZ_SRCS} INCLUDE_DIRS "." "ed25519-donna")
    # -w: 3rd-party C. -include tz_idf_compat.h: prefix hmac_sha256/aes_encrypt/
    # aes_decrypt so they don't collide with ESP-IDF's wpa_supplicant crypto.
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -w -include "${CMAKE_CURRENT_SOURCE_DIR}/tz_idf_compat.h")
else()
    add_library(trezor-crypto STATIC ${TZ_SRCS})
    set_target_properties(trezor-crypto PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
    target_include_directories(trezor-crypto PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ed25519-donna)
    target_compile_options(trezor-crypto PRIVATE -w)
endif()
