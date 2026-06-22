# nema:wifi/radio

> core (always available)  
> Package: `nema:wifi@1.0`  

WiFi radio interface: thick primitives + raw escape. Interface is @core (no namespace-level gate); each function declares its own @capability/@tier/@lease so the emitter inserts per-call gating prologues.

## Functions

| Function | Returns | Flags |
|---|---|---|
| `scan() → result<list<scan-result>, string>` | `result<list<scan-result>, string>` | `@blocking` |
| `monitor-open(channel: u8) → result<tuple<>, string>` | `result<tuple<>, string>` | — |
| `monitor-read(max: u32) → result<list<u8>, string>` | `result<list<u8>, string>` | `@blocking` |
| `monitor-close() → result<tuple<>, string>` | `result<tuple<>, string>` | — |
| `inject(channel: u8, frame: list<u8>) → result<tuple<>, string>` | `result<tuple<>, string>` | — |
| `deauth-start(bssid: string, channel: u8) → result<tuple<>, string>` | `result<tuple<>, string>` | — |
| `deauth-stop() → result<tuple<>, string>` | `result<tuple<>, string>` | — |
| `beacon-spam-start(ssid-list: list<string>) → result<tuple<>, string>` | `result<tuple<>, string>` | — |
| `beacon-spam-stop() → result<tuple<>, string>` | `result<tuple<>, string>` | — |
| `wait-event(timeout-ms: u32) → result<list<u8>, string>` | `result<list<u8>, string>` | `@blocking` |

### `scan`

Scan for visible APs. @blocking — runs on worker. Auto-grant (benign): no permission prompt. No exclusive lease required.

**Returns:** `result<list<scan-result>, string>`

### `monitor-open`

Open promiscuous/monitor mode on the given channel. Requires user permission (sensitive) and an exclusive lease.

**Parameters:**

- `channel`: `u8`

**Returns:** `result<tuple<>, string>`

### `monitor-read`

Read raw 802.11 frames from the ring buffer (up to max bytes). Drops frames when ring is full — radio never stalls.

**Parameters:**

- `max`: `u32`

**Returns:** `result<list<u8>, string>`

### `monitor-close`

Close monitor mode and release promiscuous capture.

**Returns:** `result<tuple<>, string>`

### `inject`

Inject a raw 802.11 frame on the given channel.

**Parameters:**

- `channel`: `u8`
- `frame`: `list<u8>`

**Returns:** `result<tuple<>, string>`

### `deauth-start`

Start continuous deauth loop (firmware-native, timing-critical). App receives events via wait-event; the loop itself never touches the sandbox.

**Parameters:**

- `bssid`: `string`
- `channel`: `u8`

**Returns:** `result<tuple<>, string>`

### `deauth-stop`

Stop the deauth loop.

**Returns:** `result<tuple<>, string>`

### `beacon-spam-start`

Start beacon spam (multiple fake SSIDs, firmware-native loop).

**Parameters:**

- `ssid-list`: `list<string>`

**Returns:** `result<tuple<>, string>`

### `beacon-spam-stop`

Stop the beacon spam loop.

**Returns:** `result<tuple<>, string>`

### `wait-event`

Block until the next matured radio event arrives (timeout_ms=0 → poll). Returns serialised event bytes; format is runtime-defined (JSON or binary).

**Parameters:**

- `timeout-ms`: `u32`

**Returns:** `result<list<u8>, string>`
