# Secure Element (Hardware Root-of-Trust)

> ADR 0005 — Generic, capability-gated HAL for an on-board secure element. The
> intended first consumer is a **crypto wallet** (Solana / Bitcoin / EVM).
> Capability: `secure.element`.

## Overview

Two boards ship a *different* secure-element chip — SkyRizz E32 has an **NXP SE050**
(`U18`), the dev-board has a **Microchip ATECC608B**. Rather than board-specific code,
they are exposed through one interface, `ISecureElement` (just like `IDisplayDriver`
hides e-ink vs RGB-LCD). Apps gate on `caps::Secure`, resolve the driver from the
container, and call — they never branch on the chip or the board.

The interface is a **minimal common denominator + capability-gated extras** (the same
shape as `IConfigStore` for NVS): a P-256 / TRNG / unique-id baseline every chip can do,
with richer curves and features probed before use.

## How it works

```
App ── caps().has(caps::Secure)? ──▶ container().resolve<ISecureElement>() ──▶ se->sign(...)
                                                              │
        SkyRizz E32 ── Se050Driver (I²C @0x48, reset via XL9535 P03) ─────────┤  real chip
        WASM sim    ── SimSecureElement (software, emulates SE050E) ───────────┘  dev/test
```

- **Baseline (every chip):** `randomBytes` (TRNG), `uniqueId`, `slotCount`,
  `genKey`/`publicKey`/`sign`/`verify` on **P-256**. Private keys never leave the chip.
- **Curves are probed, not assumed** — `supportsKeyType()`. This matters because **no
  crypto-wallet chain uses P-256**: BTC/EVM need **secp256k1**, Solana needs **Ed25519**.
  SE050 does secp256k1 (Ed25519 on the SE050E variant); ATECC608B does neither.
- **Optional features:** `hasFeature(SeFeature::{SecureStore,Rsa,Aes,Attestation})`.
- **Fails closed:** a not-yet-implemented op returns `false` so callers fall back to
  software crypto instead of trusting a fake result.

## Status — scaffold (ADR 0005)

The HAL, capability, and both registrations are live and build on host + wasm + esp32.
The **crypto operations are TODO**:

- `Se050Driver.init()` does real bring-up (reset pulse + I²C presence probe), but
  `genKey/sign/verify` await the NXP **Plug-and-Trust** middleware (APDU over I²C) and
  hardware to validate against.
- `SimSecureElement` has functional (deterministic, **not secure**) `randomBytes`/`uniqueId`;
  key ops await the **trezor-crypto** software core (secp256k1 / ed25519 / nist256p1),
  to be vendored in the wallet phase.

The ATECC608B driver for the dev-board is deferred — and is wallet-useless anyway (P-256 only).

## File reference

| File | Purpose |
|---|---|
| `firmware/core/include/nema/hal/secure_element.h` | `ISecureElement` interface, `SeKeyType`, `SeFeature` |
| `firmware/core/include/nema/system/capabilities.h` | `caps::Secure`, `caps::SecureStore` |
| `firmware/boards/skyrizz-e32/.../se050_driver.{h,cpp}` | SE050 backend (scaffold) |
| `firmware/platforms/wasm/include/nema/wasm/sim_secure_element.h` | Simulator backend |
| `docs/decisions/0005-secure-element-generic-hal.md` | The decision + rationale |

## Usage

```cpp
if (rt.capabilities().has(caps::Secure)) {
    if (auto* se = rt.container().resolve<ISecureElement>()) {
        if (se->supportsKeyType(SeKeyType::Secp256k1) && se->genKey(0, SeKeyType::Secp256k1))
            se->sign(0, digest, 32, sig);   // BTC/EVM key, signed inside the chip
    }
}
```

## Extending

- A new board with a secure element supplies its own `ISecureElement` and registers
  `caps::Secure` in `describeHardware()` — app/wallet code is unchanged.
- The wallet feature (HD derivation BIP32/39 + SLIP-0010, address + tx encoding) is a
  higher-level **software** layer that runs everywhere (incl. the sim) and uses the
  secure element only as an optional key-custody backend when `caps::Secure` is present.
