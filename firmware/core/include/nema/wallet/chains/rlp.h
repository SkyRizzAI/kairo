#pragma once
#include "nema/wallet/wallet_types.h"

// Minimal RLP (Recursive Length Prefix) codec for EVM transactions (Plan 94, Fase 2).
// trezor-crypto does not ship RLP. Scope: byte-strings + a single level of list, which
// is all Ethereum legacy / EIP-1559 transactions need (the access-list, itself a list,
// is passed through as a pre-encoded opaque item).

namespace nema::wallet::rlp {

// RLP-encode one byte-string.
Bytes encodeString(const uint8_t* data, size_t n);
inline Bytes encodeString(const Bytes& b) { return encodeString(b.data(), b.size()); }

// RLP-encode a list whose items are ALREADY individually encoded (string or list).
Bytes encodeList(const std::vector<Bytes>& encodedItems);

// Decode a top-level RLP list into the CONTENT (payload) of each item. For string
// items the content is the raw string; for a nested list item it is the inner
// concatenation (returned opaque). Returns false on a malformed/too-short input.
bool decodeList(const Bytes& in, std::vector<Bytes>& itemsOut);

}  // namespace nema::wallet::rlp
