# nema:wallet/wallet

> core (always available)  
> Package: `nema:wallet@1.0`  
## Functions

| Function | Returns | Flags |
|---|---|---|
| `networks() → list<string>` | `list<string>` | — |
| `ready() → bool` | `bool` | — |
| `address(network-id: string, index: u32) → result<string, string>` | `result<string, string>` | — |
| `sign-message(network-id: string, index: u32, message: string) → result<string, string>` | `result<string, string>` | — |
| `sign-transaction(network-id: string, index: u32, raw-tx-hex: string) → result<string, string>` | `result<string, string>` | — |

### `networks`

All built-in network ids the device supports.

**Returns:** `list<string>`

### `ready`

Whether a wallet exists and is unlocked (ready to read/sign).

**Returns:** `bool`

### `address`

Address for (network id, BIP44 account index). err = "locked" | "bad-network" | "derive-failed".

**Parameters:**

- `network-id`: `string`
- `index`: `u32`

**Returns:** `result<string, string>`

### `sign-message`

Sign a UTF-8 message for the account (+ on-device consent). Returns the signature as hex. err = "locked" | "rejected" | "failed".

**Parameters:**

- `network-id`: `string`
- `index`: `u32`
- `message`: `string`

**Returns:** `result<string, string>`

### `sign-transaction`

Sign a raw transaction (hex-encoded) for the account (+ on-device consent: trusted-display + physical button). Returns the signed tx as hex (the app broadcasts it itself). err = "locked" | "rejected" | "failed".

**Parameters:**

- `network-id`: `string`
- `index`: `u32`
- `raw-tx-hex`: `string`

**Returns:** `result<string, string>`
