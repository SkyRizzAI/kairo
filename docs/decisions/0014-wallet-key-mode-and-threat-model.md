# 0014 — Wallet key-mode (Seed-in-SE wrapped), threat model, and two-tier consent

- **Status:** Accepted
- **Date:** 2026-06-26
- **Area:** core/wallet, security, boards/skyrizz-e32

## Context

Plan 94 builds a hardware crypto wallet on the SkyRizz E32 (NXP SE050). Users will expect
"Trezor/Ledger-like" security, so the honest security boundary and the key-custody model must
be decided up front — they shape the whole API and the UI's truth-claims.

Two hard facts constrain the design:

1. **SE050 is a fixed-function secure element**, not a programmable one. Unlike a Ledger
   (whose certified SE runs the wallet app *and* drives the display), the SE050 cannot run
   BIP32 derivation inside the chip and has no display/buttons of its own. It is a signing/
   crypto **oracle** reached over I²C. ADR 0005 already established that no wallet chain uses
   its guaranteed P-256 curve; secp256k1 support is silicon-dependent and **Ed25519 is only
   on the SE050E variant** — so the SE cannot be the sole foundation.
2. **The ESP32-S3 is not a secure MCU.** It drives the ST7789 display and the buttons, runs
   the WiFi/BLE/USB stacks, and (in dev) boots unsigned firmware (Secure Boot v2 + Flash
   Encryption are deliberately deferred to pre-ship to keep flashing fast and brick-free).

Because the SE050 cannot do in-chip BIP32, there is a genuine tension: you cannot have *both*
a BIP39 recovery phrase (HD wallet) *and* "the key is generated in the chip and never leaves"
with this part. A decision is forced.

## Decision

**Threat model — state it honestly, never claim "Ledger-class."**
The SE050 protects **key-at-rest**: the seed/private key cannot be extracted even with
physical possession (EAL6+, tamper/side-channel resistant), and remote malware cannot
exfiltrate it. The SE050 does **not** protect **signing-intent integrity**: it is a blind
oracle that signs/unwraps whatever the ESP32 sends it, and the "trusted display" is driven by
the ESP32 — so if the ESP32 firmware is compromised, the screen can lie about what is being
signed. **The security ceiling is therefore ESP32 firmware integrity, not the SE.** This lands
the device at **Trezor Safe-class** (MCU + SE for key protection, derivation/display in MCU,
relying on Secure Boot), not Ledger-class. Mitigations: Secure Boot v2 + Flash Encryption
(pre-ship), SCP03 + host-binding ESP32↔SE050, PIN/user-presence gating, keeping the radio
stacks off the signing path. In dev (Secure Boot off) the device is effectively a hot wallet
→ **testnet/throwaway keys only** until Secure Boot is on.

**Key-mode — default is "Seed-in-SE (wrapped)."**
Of the three possible modes:
- **A. SE-native in-slot** — key generated and signed inside the SE. No recovery phrase, and
  Ed25519/Solana fails on a plain SE050. Maximum key isolation, no backup.
- **B. Seed-in-SE (wrapped)** ⭐ — a BIP39 seed is stored encrypted in NVS; the **wrapping key
  lives in the SE050** (AES/SecureStore). To sign: the SE unwraps the seed → BIP32/SLIP-0010
  derivation happens in RAM → sign → wipe.
- **C. Software/NVS** — seed encrypted by a PIN-derived key, no SE.

**B is chosen** because it is the only mode that provides a recovery phrase **and** works for
all three chains regardless of what the SE050 silicon supports (derivation + signing happen in
software for every curve). Mode A is deferred as an optional "max-isolation, single-chain,
no-backup" mode; mode C is the fallback on boards without an SE.

**Backend indicator — honest nuance.** The UI shows 🔒 "Secure Element" when mode B is active
and ⚠️ "Software key" for mode C. In mode B the *signing* still happens in software (RAM); the
SE protects the **seed-at-rest**. "Secure Element" therefore means "the seed is guarded by the
SE," not "the signature is computed inside the SE." The UI copy must not overstate this.

**PIN is a cryptographic gate, not a UI gate.** Mode C derives the seed-encryption key from the
PIN (KDF); mode B requires the PIN to unlock the SE unwrap (SE auth object). A wrong PIN
throttles. Without this, a flash dump (Secure Boot off) would expose the seed up to PIN entropy
only.

**Consent is two-tier.** Mirroring MetaMask: a persisted **permission** (`wallet.read` connect +
`wallet.sign` "may request signing", via `PermissionService`, declared in manifest `needs`)
gates *who may ask*; but **every individual signature requires per-transaction consent** on a
**system-owned** modal (rendered by GuiService, not the requesting app), approved only by a
**physical button**, **never remembered**, and **fail-closed** (Back/timeout/app-death → reject).
The modal renders the system's decode of the exact bytes to be signed (`decode == sign`) and
shows the requesting app/origin (anti-phishing). `se.raw` (sign arbitrary digest) is privileged
and never granted to third-party apps — it bypasses WYSIWYS.

## Consequences

- **Recovery works:** mode B gives users a BIP39 backup phrase and multi-chain support out of
  one seed — the expected wallet UX — at the cost of the seed touching RAM briefly at sign time.
- **Honest UX is mandatory:** the indicator and any marketing/UI copy must say Trezor-Safe-class,
  not Ledger-class; the threat model above is the reference.
- **Load-bearing:** mode B needs the SE050 to support AES wrap/unwrap or SecureStore — to be
  confirmed by the Plan 94 Fase 0 spike; if absent, mode B degrades to mode C on this silicon.
- **Load-bearing:** signing consent is never persisted and never reachable via software/RPC —
  only the physical button resolves it. Any "always allow signing" feature is forbidden.
- **Deferred:** Secure Boot v2 + Flash Encryption, SCP03 host-binding, and mode A all land
  pre-ship / later; until then the device is testnet-only.
- Supersedes nothing; builds on ADR 0005 (SE generic HAL) and ADR 0015 (wallet three-axis
  architecture).
