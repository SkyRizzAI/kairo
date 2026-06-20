# 77 — `@palanu/link` — Shared Remote Library (Forge Web ⇄ Forge CLI)

> Ekstrak inti remote (PLP codec + RemoteSession + channel + auth) dari Forge web ke **satu
> package TS framework-agnostic** `@palanu/link`, supaya **Forge web dan Forge CLI tidak
> menulis ulang hal yang sama**. Transport spesifik-environment (Web Bluetooth/Serial vs
> Node serialport/noble/TCP) tetap di consumer; logika protokol dibagi. Cermin dari
> `ILinkTransport` di firmware.

- Status: 🔴 Not started
- Depends on: 35 (PLP), **74 (auth handshake — masuk shared lib)**, 48/49 (IDL → tipe TS shared)
- Blocks: **78 (Forge CLI)**; menghapus duplikasi di Forge web

---

## 0. Keadaan sekarang

Forge web sudah punya (di `packages/forge/src/lib/`): `klp/codec.ts` (byte-identik dgn C++),
`transport/` (`VirtualCable`/`Ble`/`Serial`), `RemoteSession`, `wasmSim`. Kalau Forge CLI
(plan 78) ditulis dari nol, **codec + session + auth + channel logic ditulis dua kali** →
drift pasti terjadi (apalagi auth plan 74 menambah handshake yang rumit). Harus dibagi.

---

## 1. Goals

- [ ] Package baru **`packages/link` (`@palanu/link`)** — **TS murni, tanpa DOM/Node API**.
- [ ] Berisi: PLP **codec**, **`RemoteSession`** (orkestrasi channel), **channel handlers**
      (Screen/Log/Event/Cli/File/Ota/System), dan **auth handshake** (plan 74:
      challenge-response + token store interface).
- [ ] **`ITransport`** interface (cermin `ILinkTransport` firmware) — implementasi diinjeksi consumer.
- [ ] **Tipe command/response di-generate dari IDL** (plan 49) — shared, bukan diketik manual.
- [ ] **Forge web di-refactor** consume `@palanu/link` (bukti abstraksi benar, bukan teori).
- [ ] Forge CLI (78) tinggal pasang transport Node.

**Non-goal:** implementasi transport Node/TCP (itu di 78), UI Forge web, generator IDL itu sendiri (49).

---

## 2. Desain — pembagian tanggung jawab

```
                @palanu/link  (TS murni, isomorphic)
  ┌──────────────────────────────────────────────────────┐
  │ codec (PLP frames, CRC8, RLE)                         │
  │ RemoteSession  — channel mux, handshake, token        │
  │ AuthClient     — challenge→HMAC→token (plan 74)        │
  │ channels: Screen/Log/Event/Cli/File/Ota/System/Ext    │
  │ ITransport { open(); send(bytes); onData(cb); close()}│  ← cermin ILinkTransport (C++)
  │ types/*  — DIGENERATE dari api/*.pidl (plan 49)        │
  └──────────────────────────────────────────────────────┘
        ▲ consume                         ▲ consume
  packages/forge (web)            packages/forge-cli (node)  ← plan 78
  WebBluetoothTransport           SerialPortTransport (serialport)
  WebSerialTransport              NobleBleTransport (noble)
  VirtualCableTransport (wasm)    TcpTransport (net.Socket + mDNS)
  WebSocketTransport ◄──── network (plan 75) ────► WebSocketTransport (ws)
```

> **WebSocket = transport network bersama** (plan 75): browser pakai `WebSocket` native,
> Node pakai `ws`. Implementasinya tipis & isomorphic, jadi **boleh tinggal di `@palanu/link`**
> (dengan `WebSocket` di-resolve dari `globalThis`/inject) — inilah yang membuat **Forge web
> DAN Forge CLI sama-sama bisa remote via jaringan** tanpa nulis dua kali.

- **Aturan emas:** `@palanu/link` **tak boleh** `import` apa pun browser-only atau node-only.
  Hanya `Uint8Array`/`Promise`/event-emitter ringan. Transport = interface, di-inject.
- `ITransport` sengaja disamakan bentuknya dgn `ILinkTransport` C++ → satu mental model
  device & host.
- **AuthClient** di sini supaya logika handshake (plan 74) ditulis sekali; token store
  abstrak (`ITokenStore`) — web pakai `localStorage`, CLI pakai `~/.palanu/config.json`.

## 3. Tasks
- [ ] Scaffold `packages/link` (package.json `@palanu/link`, tsconfig, build).
- [ ] Pindahkan `codec.ts` + `RemoteSession` + channel logic dari `packages/forge/src/lib`.
- [ ] Definisikan `ITransport` + `ITokenStore` + `AuthClient` (handshake plan 74).
- [ ] Wire generator IDL (plan 49) → `@palanu/link/types` (command/response shapes).
- [ ] Refactor Forge web: transport browser implement `ITransport`, sisanya pakai shared lib.
- [ ] Tes unit codec (vektor sama dengan `firmware/tests/klp`) di package ini.
- [ ] `bun run test` + Forge web tetap jalan (simulator + /remote tak regresi).

## 4. Acceptance criteria
- [ ] `@palanu/link` lulus `tsc` tanpa referensi DOM/Node (cek: build dalam env netral).
- [ ] Forge web pakai `@palanu/link`; **tak ada** salinan codec/session lokal lagi.
- [ ] Vektor uji codec identik dengan C++ (`firmware/tests/klp`) — byte-for-byte.
- [ ] Auth handshake (plan 74) ada di shared lib, dipakai web; siap dipakai CLI (78).
- [ ] Menambah transport baru = implement `ITransport` saja, nol perubahan core lib.
