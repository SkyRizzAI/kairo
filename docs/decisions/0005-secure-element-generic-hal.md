# 0005 — Secure element is a generic capability-gated HAL, not board code

- **Status:** Accepted
- **Date:** 2026-06-21
- **Area:** core/hal, security, wallet

## Context

Two boards already ship a hardware secure element, and they are **different chips**:

- **SkyRizz E32** — NXP **SE050** (`U18`, shared I²C on `GPIO47`/`GPIO48`, reset via
  XL9535 P03). Confirmed in `refs/dev-board-1-pin_map.md` (§ Secure element) and the
  board's `board_config.h` (`P0_SE_RST`).
- **dev-board** — Microchip **ATECC608B** (`PIN_SE_EN` GPIO8, I²C `0x60`, 100 kHz).

Today the chips are soldered and their pins are mapped, but neither is exposed to the
runtime — there is no driver and no app-facing API. The only crypto in the tree is
software SHA-256 (`nema/crypto/sha256.h`), used to hash profile passwords.

The question raised: should a secure element be a **generic cross-board API** (like
`IDisplayDriver`, which hides e-ink vs RGB-LCD) or stay **board-specific**? The fact
that two boards already carry two different chips is exactly the "one core, many boards"
situation the project is built around, and CLAUDE.md forbids branching on board type —
apps must gate on capabilities.

A complication that display does not have: secure elements are **not feature-uniform**.
SE050 is rich (RSA, AES, secure storage, attestation, many slots); ATECC608B is
ECC-focused with a small slot count. A naïve "full feature parity" interface would be
unimplementable on the smaller chip.

## Decision

Add a generic **`ISecureElement`** HAL interface in core
(`firmware/core/include/nema/hal/secure_element.h`), modeled on the existing
interface-in-core / impl-per-board pattern (`IDisplayDriver`, `IConfigStore`).

The interface is a **minimal common denominator + capability-gated extras**, mirroring
how `IConfigStore` exposes only `getString/getInt/...` rather than all of NVS:

- **Baseline (guaranteed on every chip):** `randomBytes` (TRNG), `uniqueId`, `slotCount`,
  `genKey`/`publicKey`/`sign`/`verify` on NIST **P-256** ECC. Private keys are generated
  and used inside the chip and never leave it.
- **Optional (SE050-only):** queried via `hasFeature(SeFeature::{SecureStore,Rsa,Aes,
  Attestation})`; the richer calls are deferred until a consumer needs them.

**Curve reality for the crypto-wallet use case.** The first real consumer is a
crypto wallet (Solana / Bitcoin / EVM). Crucially, **no wallet chain uses P-256** —
Bitcoin and EVM use **secp256k1**, Solana uses **Ed25519**. So P-256 stays the
guaranteed baseline, but `SeKeyType` is extended with `Secp256k1` and `Ed25519`, and
callers MUST probe `supportsKeyType()` first: SE050 does secp256k1 (Ed25519 only on the
SE050E variant); ATECC608B does *neither*. The consequence is that the secure element
is **not the foundation of the wallet** — the wallet needs a software crypto core to
work in the simulator, on boards without an SE, and for Ed25519 regardless. The SE is an
optional key-custody backend. The chosen software crypto core is **trezor-crypto** (the
crypto library from Trezor hardware wallets — secp256k1, ed25519, nist256p1, BIP32/39,
keccak, base58; MIT; embedded-proven), to be vendored in the wallet phase.

Two capability strings are added to `nema/system/capabilities.h`:

- `caps::Secure` = `"secure.element"` — board has a hardware root-of-trust.
- `caps::SecureStore` = `"secure.store"` — SE050-only secured key-value.

Apps consume it the standard way: check `rt.capabilities().has(caps::Secure)`, then
`rt.container().resolve<ISecureElement>()`. No board-name branching.

**Drivers are SCAFFOLDED, not fully implemented.** Two backends ship now so the HAL has
real registrations and apps can already resolve it:

- `SimSecureElement` (WASM, `platforms/wasm/.../sim_secure_element.h`) — emulates an
  SE050E (all wallet curves report supported) for in-browser wallet development.
  `randomBytes`/`uniqueId` are functional (deterministic, sim-only); key ops are TODO.
- `Se050Driver` (SkyRizz, `boards/skyrizz-e32/.../se050_driver.{h,cpp}`) — `init()` does
  the real bring-up (reset via XL9535 P03 + I²C presence probe @0x48); curve/feature
  probes reflect the silicon; the crypto ops are TODO pending the NXP Plug-and-Trust
  middleware (APDU session over I²C) and hardware to validate against.

Both register via `rt.container().registerAs<ISecureElement>()` + `caps::Secure` in their
`describeHardware()` / `registerDrivers()`. The dev-board `atecc608b_driver` (likely in
`platforms/esp32`, since that chip also appears there) is still deferred — and is wallet-
useless anyway (P-256 only). All scaffolded ops **fail closed** (return false) so a caller
falls back to software crypto rather than trusting a fake result.

## Consequences

- **Enables** apps to use a hardware root-of-trust portably; on boards without one they
  gate on `caps::Secure` and fall back to software crypto (`nema/crypto/sha256.h`).
- **Complements, does not replace, NVS.** `IConfigStore` stays the store for non-secret
  config; the secure element holds non-exportable keys. The intended pairing is
  NVS-stores-ciphertext, SE-holds-the-key.
- **Load-bearing:** the baseline (P-256 ECC + TRNG + uniqueId) must remain implementable
  on the *smallest* chip (ATECC608B). Anything a chip might lack goes behind
  `hasFeature()` — never promote a chip-specific op into the baseline.
- **Deferred:** all drivers, any runtime accessor (`rt.secureElement()`), RSA/AES/secure-
  storage method signatures, and the WASM/simulator stub. They land when the first
  consumer does, not before.
- The header is interface-only (no `.cpp`), so core's explicit-source CMake list is
  unchanged.
