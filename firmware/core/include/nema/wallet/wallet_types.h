#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Core wallet value types (Plan 94, ADR 0015). Hardware-agnostic; no crypto impl
// here — these are the shared vocabulary for the three wallet axes
// (IWalletBackend custody · IChain protocol · consent modal).

namespace nema::wallet {

using Bytes = std::vector<uint8_t>;

// Signature curves a wallet key can use.
//   Secp256k1 → Bitcoin / EVM (ECDSA, sign a 32-byte digest).
//   Ed25519   → Solana       (EdDSA, sign the whole message).
enum class Curve : uint8_t { Secp256k1, Ed25519 };

// Which custody backend holds a key — drives the UI trust indicator (ADR 0014).
//   Software      → ⚠️ seed encrypted in software (PIN-derived key).
//   SecureElement → 🔒 seed wrapped by the SE050 (mode B).
enum class BackendKind : uint8_t { Software, SecureElement };

// A BIP32 / SLIP-0010 derivation path. Each element is a child index; the
// hardened bit is OR-ed into the value (e.g. 44' == 44u | DerivationPath::Hardened).
struct DerivationPath {
    static constexpr uint32_t Hardened = 0x80000000u;
    std::vector<uint32_t> indices;
};

// Raw public key bytes. Format is chain-specific (SEC1 for secp256k1; 32B for Ed25519).
using PubKey = Bytes;

// Raw signature bytes (secp256k1: r||s, 64B [+recovery id for EVM]; Ed25519: 64B).
using Signature = Bytes;

// Chain protocol-family metadata.
struct ChainInfo {
    const char* family;        // "evm" | "bitcoin" | "solana"
    uint32_t    bip44CoinType; // 60 evm, 0 btc, 501 sol
    Curve       curve;
};

// A concrete network within a family. Networks are DATA (NetworkRegistry), not code:
// EVM networks differ only by chainId.
struct NetworkParams {
    const char* id;       // "eth-mainnet" | "bnb" | "polygon" | "btc-testnet" | "sol-devnet"
    const char* family;   // selects the IChain
    const char* label;    // human name for the UI
    uint64_t    chainId;  // EVM chain id; 0 for non-EVM families
    bool        testnet;
};

// One unit of signing work. A single tx may yield several (BTC PSBT = one per input).
//   secp256k1/ECDSA: payload = 32-byte digest, prehashed = true.
//   ed25519/EdDSA:   payload = full message,   prehashed = false.
struct SigningItem {
    DerivationPath path;
    Curve          curve;
    Bytes          payload;
    bool           prehashed;
};

// A wallet account = one address under a network, derived at a BIP44 index.
struct Account {
    std::string walletId;   // which seed
    std::string networkId;  // NetworkParams::id
    uint32_t    index;      // BIP44 account index
    std::string address;    // derived/display address
    std::string label;
    BackendKind backend;    // custody of the underlying key (for the indicator)
};

// One labelled row in the transaction-confirm modal (WYSIWYS).
struct PreviewRow {
    std::string label;  // "To", "Amount", "Fee", ...
    std::string value;
};

// Human-readable decode of a transaction for the consent modal.
struct TxPreview {
    std::vector<PreviewRow> rows;
    bool        blindSign = false;  // true → could not fully decode; show warning
    std::string warning;            // optional extra warning text
};

// Kind of message being signed (affects decode + display).
enum class MsgKind : uint8_t { Personal, Eip712, SolanaMessage, Raw };

// Human-readable decode of a message for the consent modal.
struct MsgPreview {
    std::string text;               // decoded/displayable message
    bool        blindSign = false;
};

// ── Permission strings (consumed by PermissionService; declared in manifest `needs`) ──
// These are PERMISSIONS (per-app grants, Plan 87) — distinct from caps::Secure, which is a
// hardware capability. Signing always also requires per-transaction consent (ADR 0014).
namespace perms {
    inline constexpr const char* Read  = "wallet.read"; // connect, list accounts/addresses
    inline constexpr const char* Sign  = "wallet.sign"; // may REQUEST signing (consent still per-tx)
    inline constexpr const char* SeRaw = "se.raw";      // privileged raw SE signing — internal only
}

} // namespace nema::wallet
