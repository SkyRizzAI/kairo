# 0015 — Wallet architecture: three orthogonal axes (Custody × Chain × Consent)

- **Status:** Accepted
- **Date:** 2026-06-26
- **Area:** core/wallet, security

## Context

Plan 94 adds a hardware crypto wallet (Bitcoin, Solana, EVM) on top of the secure-element
HAL from ADR 0005. Three concerns collide in a wallet and are easy to conflate:

1. **Where the key lives and signs** — SE050 (hardware) vs software/NVS. ADR 0005 already
   established the secure element is an *optional* key-custody backend, not the foundation
   (no wallet chain uses the SE050's guaranteed P-256 curve; the software crypto core is
   load-bearing).
2. **How a chain works** — address format, transaction serialization, what bytes get hashed,
   how to decode a transaction for on-screen display. Bitcoin (UTXO/PSBT, secp256k1),
   Solana (Ed25519, base58), and EVM (RLP/keccak, secp256k1) are mutually different. EVM
   *networks* (Ethereum, BNB Smart Chain, Polygon, Arbitrum, Base, …) are NOT different
   from each other in code — they differ only by `chainId`.
3. **How the user consents** — the trusted-display + physical-button confirmation that is
   the entire security guarantee of a hardware wallet.

If these are entangled (e.g. a "Bitcoin-on-SE050" class separate from "Bitcoin-on-software"),
the implementation explodes combinatorially: `chains × backends × …`. Adding a chain would
mean touching custody code; adding a custody backend would mean touching every chain.

## Decision

Model the wallet as **three orthogonal axes that compose**, never multiply. Each is a
separate interface in `firmware/core/include/nema/wallet/`, blind to the other two.

- **CUSTODY — `IWalletBackend`** (curve-level, chain-blind). Knows only curves
  (`Secp256k1`/`Ed25519`). API: `publicKey(path, curve)` and
  `sign(path, curve, payload, prehashed)`. **Never exposes the private key or seed.** Two
  implementations: `SeBackend` (seed wrapped by SE050) and `NvsBackend` (seed encrypted in
  software). Both derive BIP32/SLIP-0010 from their seed internally — derivation stays
  inside the backend so the seed never crosses the API boundary.

- **CHAIN — `IChain`** (protocol-level, custody-blind). One implementation per *protocol
  family*: `EvmChain`, `BitcoinChain`, `SolanaChain`. API: `addressFromPubkey`, `pathFor`
  (BIP44), `decodeForDisplay` (WYSIWYS), `buildSigningPayload` → **list** of `SigningItem`
  (BTC PSBT has one per input; EVM/SVM one), `encodeSigned` ← **list** of `Signature`,
  `decodeMessage`. **Networks are DATA, not code:** a `NetworkRegistry` of `NetworkParams`
  (`{id, family, chainId, testnet}`) maps each network to a family + parameters. MVP: an
  embedded `constexpr` table (trusted, no injection).

- **CONSENT — system-owned modal** (see ADR 0014). `WalletService` orchestrates:
  `chain.decodeForDisplay → consent modal → backend.sign(each item) → chain.encodeSigned`.

The signing payload carries a `prehashed` flag because the two curves sign differently:
secp256k1/ECDSA signs a 32-byte digest (`prehashed=true`, RFC6979 + low-S); Ed25519/EdDSA
signs the **whole message** (`prehashed=false`, hashes internally). Getting this wrong is a
classic wallet bug, so it is encoded in the interface, not left to the caller.

**Bridge — one core, many consumers.** Native C++ apps, on-device JS/WASM apps (host binding
`nema.wallet.*`), and web dApps (provider shim implementing EIP-1193/6963, Wallet Standard,
signPsbt — forwarded over the existing PLP/Forge transport) all funnel through one
`WalletRequestRouter` → one `WalletService` → one consent modal. One security path, not three.

## Consequences

- **Add a chain family** (e.g. Aptos, Cosmos) = +1 `IChain`; it works with every backend
  automatically.
- **Add an EVM network** (e.g. zkSync, Linea) = +1 row in `NetworkRegistry`; zero code.
- **Add a custody backend** (e.g. a future SE chip) = +1 `IWalletBackend`; it works with
  every chain automatically.
- **Load-bearing:** the seed/private key **never** crosses `IWalletBackend`'s boundary —
  derivation happens inside the backend. Any future method that would return key material
  violates the wallet's core guarantee and must be rejected.
- **Load-bearing:** `decode == sign` — the consent modal renders the decode of the exact
  bytes `buildSigningPayload` produces; never decode one thing and sign another.
- The device is a **signer, not a broadcaster** — pushing a signed tx to the network is the
  app/dApp's job; the wallet holds no RPC node.
- **Deferred:** clear-signing decoders are incremental (v1 clear-signs native + ERC-20
  transfers; complex contract calls are blind-signed with a warning). SE-native in-slot
  custody (mode A in ADR 0014) is deferred — these axes only describe the structure.
- Interfaces are header-only contracts (like `ISecureElement`), so core's explicit-source
  CMake list is unchanged until the implementations land.
