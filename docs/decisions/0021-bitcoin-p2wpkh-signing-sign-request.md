# 0021 — Bitcoin P2WPKH signing via a compact sign-request (not PSBT)

- Status: accepted
- Date: 2026-06-29

## Context

`BitcoinChain` shipped with address derivation only; tx/message signing failed
closed (ADR 0015 / bitcoin.h). The wallet now needs to actually sign Bitcoin —
the Web3 Test app's "Sign transaction" must work on BTC, not just EVM/Solana.

Two facts shaped the design:

1. **BIP143 needs data a bare unsigned tx lacks.** The SegWit sighash binds each
   input's **amount** and **scriptCode** — neither is present in a legacy unsigned
   transaction. The standard carrier for this is **PSBT**, but a correct PSBT
   parser is a large, error-prone surface for a device whose only job here is to
   sign a single-key P2WPKH spend.
2. **A wrong sighash loses funds.** bitcoin.h has always warned that signing must
   be verified against the official BIP143 vector before it touches real value.

## Decision

**Sign native-SegWit P2WPKH with BIP143 over a compact, explicit "Palanu BTC v1"
sign-request, verified against the official BIP143 test vector — not PSBT.**

- The app passes a self-describing blob: format byte, `nVersion`, then per input
  `{ prev txid, vout, amount (sats), scriptPubKey, nSequence }`, then per output
  `{ amount, scriptPubKey }`, then `nLockTime`. Everything BIP143 needs is in the
  blob, so the offline signer never has to fetch prevouts.
- `decodeForDisplay` renders **To / Amount / Fee** from the same blob (WYSIWYS):
  fee = Σ inputs − Σ outputs. It is **not** blind-signing.
- `buildSigningPayload` emits one SIGHASH_ALL sighash per input;
  `encodeSigned` assembles the witness tx, **recovering the pubkey from the
  signature** (`r||s||recid` → `ecdsa_recover_pub_from_sig` → compressed), so the
  IChain `encodeSigned(sigs)` contract needs no extra pubkey plumbing.
- Correctness is locked by `firmware/tests/bitcoin_chain_test.cpp`: the BIP143
  worked example reproduces digest
  `c37af3…78cb670` exactly. CI fails if the sighash ever drifts.

## Consequences

- **BTC sending works** for the common case (single-key P2WPKH / BIP84) across
  device (trezor-crypto) and simulator, 1=1 with EVM/Solana.
- **Deliberately out of scope, failing closed** (blind-sign / empty payload, never
  signing blind): non-SegWit P2PKH inputs, nested-SegWit & Taproot, multi-key /
  full PSBT, and BIP-322 message signing (legacy `signmessage` is used instead).
- The sign-request is a **Palanu-internal format**, not interoperable with
  external PSBT tooling. If interop is later needed, a PSBT→sign-request adapter
  can be added without touching the verified sighash core.
- Pubkey-by-recovery assumes the signature carries a valid recid (it does — the
  secp256k1 backend returns `r||s||recid`); a malformed sig makes `encodeSigned`
  fail closed (empty tx) rather than emit a bad witness.
