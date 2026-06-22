# 77 — `@palanu/link` — Shared Remote Protocol Library

> Ekstrak inti protokol remote (PLP codec + `RemoteSession` + channel handlers + auth
> handshake) dari Forge web ke **satu package TypeScript murni** `@palanu/link`, supaya
> **Forge web dan Forge CLI tidak menulis ulang hal yang sama**. Transport spesifik-
> environment (Web Bluetooth/Serial untuk browser, `serialport`/`ws` untuk Node) tetap
> di consumer; logika protokol dibagi. Cermin dari `ILinkTransport` di firmware C++.

- Status: ✅ DONE — package `@palanu/link` extracted, IDL wired, Forge web refactored. Build green (tsc + tests + forge:build).
- Depends on: 35 (PLP — sudah done), 48/49 (IDL pipeline — untuk generate PLP types),
  74 (auth — `RemoteSession` sudah handle `AUTH_*` opcodes, pakai yang ada)
- Blocks: **78 (Forge CLI)**; menghapus duplikasi di Forge web

---

## 0. Keadaan sekarang

Forge web sudah punya implementasi PLP lengkap di `packages/forge/src/lib/`:

| File | Isi | Pure TS? |
|---|---|---|
| `plp/codec.ts` | PLP frame codec: `Channel` enum, `crc8`, `encodeFrame`, `FrameParser`, RLE | ✅ 0 import DOM/Node |
| `plp/transport.ts` | `ILinkTransport` interface + `loopbackPair()` | ✅ |
| `plp/uuids.ts` | BLE GATT UUID string | ✅ |
| `plp/codec.test.ts` | Unit test codec (bun:test) | ✅ |
| `RemoteSession.ts` | Orkestrasi semua 10 channel + auth handshake | ⚠️ `localStorage` hardcoded 3× |
| `transport/BleTransport.ts` | Web Bluetooth | ❌ browser-only |
| `transport/SerialTransport.ts` | Web Serial | ❌ browser-only |
| `transport/WebSocketTransport.ts` | Browser WebSocket (plan 75) | ❌ browser-only |
| `transport/VirtualCableTransport.ts` | WASM sim cable | ❌ DOM/WASM |
| `components/*.svelte` | UI terminal/file browser | ❌ Svelte UI |

**Gap**: kalau Forge CLI (plan 78) ditulis dari nol, codec + session + auth + channel
logic ditulis dua kali → drift pasti terjadi. Harus dibagi.

**Blocker ekstraksi**: `RemoteSession.ts` pakai `localStorage` langsung untuk token auth
(3 method private: `#savedToken`/`#saveToken`/`#clearToken`, key `palanu.remote.token`).
Tidak ada interface — storage backend hardcoded. Solusi: abstraksi `ITokenStore`.

---

## 1. Goals

- [ ] Package baru **`packages/link` (`@palanu/link`)** — **TS murni, tanpa DOM/Node API**
      (isomorphic). Hanya `Uint8Array`/`Promise`/`TextEncoder`/`TextDecoder`/`crypto.subtle`.
- [ ] Berisi: PLP **codec**, **`RemoteSession`** (orkestrasi 10 channel), **channel handlers**
      (Control/Screen/Input/Log/System/Ota/Ext/Event/Cli/File), **auth handshake** (opcodes
      `AUTH_CHALLENGE`/`AUTH_RESPONSE`/`AUTH_OK`/`AUTH_FAIL`/`AUTH_REQUIRED` sudah ada di
      `RemoteSession`).
- [ ] **`ILinkTransport`** interface (sudah ada di `plp/transport.ts` — cermin `ILinkTransport`
      firmware) — implementasi diinjeksi consumer.
- [ ] **`ITokenStore`** interface baru — abstraksi token storage. Default:
      `LocalStorageTokenStore` (browser). Siap untuk `FileTokenStore` (CLI, plan 78).
- [ ] **Tipe PLP command/response di-generate dari IDL** (plan 48/49) — shared, bukan
      hand-typed di `codec.ts`.
- [ ] **Forge web di-refactor** consume `@palanu/link` (bukti abstraksi benar, bukan teori).
- [ ] Forge CLI (78) tinggal pasang transport Node + `FileTokenStore`.

**Non-goal:** implementasi transport Node (itu di 78), UI Forge web, generator IDL itu
sendiri (49 — sudah done, tinggal wire PLP types), `AuthClient` full baru (74 DITUNDA —
`RemoteSession` sudah handle auth opcodes, pakai yang ada).

---

## 2. Desain — pembagian tanggung jawab

```
                @palanu/link  (TS murni, isomorphic — 0 DOM/Node)
  ┌──────────────────────────────────────────────────────────────┐
  │ codec          PLP frames, CRC8, RLE, Channel enum           │
  │ transport      ILinkTransport interface + loopbackPair()     │
  │ uuids          BLE GATT UUIDs (string konstan)               │
  │ session        RemoteSession — 10 channel mux + handshake    │
  │                + auth (AUTH_* opcodes, challenge→HMAC→token) │
  │ tokens         ITokenStore interface                         │
  │ types/*        DIGENERATE dari api/*.pidl (plan 48/49)        │
  └──────────────────────────────────────────────────────────────┘
        ▲ consume                                  ▲ consume
  packages/forge (web)                   packages/forge-cli (node)  ← plan 78
  LocalStorageTokenStore                 FileTokenStore (~/.palanu/config.json)
  BleTransport (Web Bluetooth)           NodeSerialTransport (serialport)
  SerialTransport (Web Serial)           NodeWebSocketTransport (ws)
  WebSocketTransport (browser WS)        —
  VirtualCableTransport (WASM sim)       —
```

### Aturan emas

- `@palanu/link` **tidak boleh** `import` apa pun browser-only atau node-only. Hanya
  `Uint8Array`/`Promise`/`TextEncoder`/`TextDecoder`/`crypto.subtle`/`setInterval` —
  semua cross-runtime.
- Transport = interface (`ILinkTransport`), di-inject via konstruktor `RemoteSession`.
  Consumer bertanggung jawab bikin transport-nya sendiri.
- `ITokenStore` di-inject via konstruktor `RemoteSession`. Default
  `LocalStorageTokenStore` untuk backward compat Forge web. CLI (78) pakai
  `FileTokenStore`.
- `ILinkTransport` sengaja disamakan bentuknya dengan `ILinkTransport` C++ → satu mental
  model device & host.

### Refactor `RemoteSession.ts` — satu-satunya blocker

Sebelum ekstrak, refactor 3 method private yang pakai `localStorage`:

```ts
// Sebelum (hardcoded):
#savedToken(): string | null {
  try { return typeof localStorage !== 'undefined' ? localStorage.getItem(TOKEN_KEY) : null; }
  catch { return null; }
}

// Sesudah (abstracted via ITokenStore):
interface ITokenStore {
  get(key: string): string | null;
  set(key: string, val: string): void;
  remove(key: string): void;
}

class LocalStorageTokenStore implements ITokenStore { /* localStorage-backed */ }
// CLI nanti: class FileTokenStore implements ITokenStore { /* file-backed */ }

// RemoteSession constructor:
constructor(transport: ILinkTransport, tokenStore: ITokenStore = new LocalStorageTokenStore()) { ... }
```

`TOKEN_KEY` (`palanu.remote.token`) jadi konstanta default, bisa di-override via konstruktor.

---

## 3. Tasks

### Tier 1 — ekstrak verbatim (zero changes, pure TS)
- [ ] Scaffold `packages/link/` — `package.json` (`@palanu/link`, private, type: module),
      `tsconfig.json` (strict, noEmit, moduleResolution: bundler), build script.
- [ ] Pindahkan `plp/codec.ts` → `@palanu/link/codec` (Channel enum, crc8, encodeFrame,
      FrameParser, rleEncode, rleDecode).
- [ ] Pindahkan `plp/transport.ts` → `@palanu/link/transport` (`ILinkTransport` + `loopbackPair`).
- [ ] Pindahkan `plp/uuids.ts` → `@palanu/link/uuids` (BLE GATT UUIDs).
- [ ] Pindahkan `plp/codec.test.ts` → `@palanu/link/codec.test.ts`.

### Tier 2 — ekstrak dengan 1 refactor
- [ ] Definisikan `ITokenStore` interface + `LocalStorageTokenStore` default adapter.
- [ ] Refactor `RemoteSession.ts`: ganti 3 method `localStorage` → `ITokenStore` inject.
- [ ] Pindahkan `RemoteSession.ts` → `@palanu/link/session`.
- [ ] Pindahkan tipe/interface yang dipakai `RemoteSession` (`ScreenFrame`, `LogEntry`,
      `EventEntry`, `CliChunk`, `FileEntry`, `BoardProfile`, `Key`, `KEY_MAP`, `frameDims`)
      ke `@palanu/link/types`.

### Tier 3 — wire IDL generation
- [ ] Tambah definisi PLP channel/command/response ke `api/*.pidl` (baru — saat ini IDL
      cuma cover Nema System API, bukan PLP transport).
- [ ] Extend `packages/idl/src/emit/` dengan emitter TypeScript untuk PLP types
      (channel opcodes, command/response shapes).
- [ ] Output: `@palanu/link/types/generated` — jangan edit manual.

### Tier 4 — refactor Forge web
- [ ] Ganti import `$lib/plp/codec` → `@palanu/link/codec` di semua file Forge web.
- [ ] Ganti import `$lib/plp/transport` → `@palanu/link/transport`.
- [ ] Ganti import `$lib/RemoteSession` → `@palanu/link/session`.
- [ ] Hapus file lokal yang sudah dipindah (`plp/codec.ts`, `plp/transport.ts`,
      `plp/uuids.ts`, `RemoteSession.ts`).
- [ ] Browser transports (`Ble/Serial/WebSocket/VirtualCable`) tetap di Forge web,
      implement `ILinkTransport` dari `@palanu/link`.
- [ ] `remoteLink.ts`, `wasmSim.ts`, `simStore.svelte.ts`, `utils.ts`, `components/*`,
      `trpc/*` — tetap di Forge web (bukan scope ekstraksi).

### Tier 5 — verify
- [ ] Unit test codec — vektor identik dengan `firmware/tests/klp` (C++). Byte-for-byte.
- [ ] `bun run test` lulus.
- [ ] Forge web tetap jalan: simulator (`/simulator`) + remote (`/remote`) tak regresi.
- [ ] `@palanu/link` lulus `tsc` tanpa referensi DOM/Node (build dalam env netral —
      cek: tidak ada `lib: ["dom"]` di tsconfig, tidak ada `@types/node`).

---

## 4. Acceptance criteria

- [ ] `@palanu/link` lulus `tsc` tanpa referensi DOM/Node (cek: build dalam env netral).
- [ ] Forge web pakai `@palanu/link`; **tak ada** salinan codec/session/transport lokal
      lagi (browser transports tetap, tapi import `ILinkTransport` dari shared lib).
- [ ] Vektor uji codec identik dengan C++ (`firmware/tests/klp`) — byte-for-byte.
- [ ] `ITokenStore` ter-inject di `RemoteSession`; `LocalStorageTokenStore` default;
      siap untuk `FileTokenStore` (CLI, plan 78).
- [ ] PLP types generated dari IDL, bukan hand-typed di `codec.ts`.
- [ ] Auth handshake (`AUTH_*` opcodes) ada di shared lib, dipakai Forge web; siap dipakai
      CLI (78) tanpa perubahan.
- [ ] Menambah transport baru = implement `ILinkTransport` saja, nol perubahan core lib.
- [ ] Semua 10 channel (Control/Screen/Input/Log/System/Ota/Ext/Event/Cli/File) tetap
      berfungsi di Forge web setelah refactor.

---

## 5. Struktur package target

```
packages/link/
├── package.json          # @palanu/link, private, type: module
├── tsconfig.json         # strict, noEmit, moduleResolution: bundler
├── src/
│   ├── codec.ts          # PLP frame codec (dari forge/src/lib/plp/codec.ts)
│   ├── transport.ts      # ILinkTransport + loopbackPair (dari plp/transport.ts)
│   ├── uuids.ts          # BLE GATT UUIDs (dari plp/uuids.ts)
│   ├── session.ts        # RemoteSession (dari RemoteSession.ts, refactored)
│   ├── tokens.ts         # ITokenStore + LocalStorageTokenStore (BARU)
│   ├── types.ts          # ScreenFrame/LogEntry/CliChunk/FileEntry/BoardProfile/...
│   └── types-generated/  # IDL output (jangan edit manual)
└── test/
    └── codec.test.ts     # vektor uji byte-for-byte vs C++
```

## 6. Catatan

- **Plan 74 (auth) DITUNDA** — tapi `RemoteSession` sudah handle `AUTH_*` opcodes
  (challenge-response + HMAC via `crypto.subtle`). Jadi auth handshake **sudah jalan**
  di shared lib tanpa menunggu 74 selesai penuh. Yang belum done di 74: device-side
  `RemoteAuthStore` (password hashing, token management, rate limiting) — itu sisi
  device, bukan shared lib.
- **KLP → PLP rename** sudah done di kode (codec.ts pakai `Channel` enum, bukan `KLP`).
  Plan docs lama mungkin masih sebut KLP — abaikan, kode sudah PLP.
- **WebSocket transport**: plan lama bilang "boleh tinggal di `@palanu/link`". MVP ini
  **tidak** memindahkan WebSocket transport ke shared lib — implementasi browser
  (`WebSocketTransport.ts`) tetap di Forge web, implementasi Node (`NodeWebSocketTransport`)
  dibuat di Forge CLI (78). Keduanya implement `ILinkTransport` yang sama. Kalau nanti
  terbukti duplikatif, bisa di-isomorphic-kan.
