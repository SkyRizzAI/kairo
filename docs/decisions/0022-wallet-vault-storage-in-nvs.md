# 0022 — Store the wallet vault in NVS, not the LittleFS /system partition

- Status: accepted
- Date: 2026-07-01

## Context

The wallet vault (plaintext index + PIN-encrypted, optionally SE-wrapped seed
blobs) was persisted through `AppStorageKvStore` → `AppStorage(critical=true)` →
files under `/system/data/com.palanu.wallet/` on the **LittleFS `/system`
partition**.

That partition does double duty: it also holds the read-only **asset image**
(`firmware/assets/` → fonts, anims), which `main/CMakeLists.txt` builds with
`littlefs_create_partition_image(... FLASH_IN_PROJECT)`. `FLASH_IN_PROJECT` puts
that image into the flash argument list, so a full **`idf.py flash` rewrites the
entire `/system` partition on every firmware update** — wiping all user data in
it, including the wallet. Symptom: after each reflash the launcher shows "Create
new wallet" again, even with a working Secure Element. (A plain power-off/restart
did NOT lose the wallet — LittleFS writes commit on `fclose`, and the SE seal key
persists in the chip. Only a full flash, or a `format_if_mount_failed` reformat
after FS corruption, wiped it.)

Read-only assets and user data sharing one re-flashable partition is the root
cause. Options considered: (a) always `app-flash` in dev — a workflow band-aid,
full flash still wipes; (b) a separate LittleFS user-data partition — correct but
needs a partitions.csv change + a one-time repartition flash; (c) move the vault
to NVS.

## Decision

**Persist the wallet vault in NVS via a new `ConfigKvStore : IKvStore`** (adapting
`IConfigStore`, which is NVS-backed on device), replacing `AppStorageKvStore`.

- The `nvs` partition is **not** part of the flash image, so `idf.py flash` never
  touches it — wallets survive firmware updates and a LittleFS reformat.
- `NvsConfigStore::setString` calls `nvs_commit` immediately, so writes are durable
  across a restart the instant they're made.
- `IConfigStore` is string-only, so binary blobs are **hex-encoded**; NVS keys are
  capped at 15 chars, so each key is hashed to 8 hex chars (djb2).
- Security is unchanged: NVS holds only ciphertext. The seed is still
  PIN-encrypted (AES-256-CBC over `MAGIC||seed`) and, with a secure element,
  additionally SE-wrapped (device-bound). NVS is not a confidentiality boundary
  here — the PIN + SE are.

## Consequences

- **Wallets now persist across firmware flashes and restarts.** The "disappears on
  reflash" bug is fixed at the storage layer, not just the flash workflow.
- **One-time migration cost:** existing LittleFS wallets are not read by the new
  NVS store, so a device with an old wallet shows "Create new wallet" once and the
  user re-creates it (acceptable: dev keys are testnet/throwaway, and those wallets
  were already being wiped by reflashes). No automatic LittleFS→NVS migration was
  added; it can be a follow-up if a production device ever needs it.
- **NVS space:** the vault is tiny (index ≈ hundreds of bytes; each seed blob
  ≈112–200 bytes hex-encoded to ≈224–400 chars), well within the 24 KB `nvs`
  partition shared with other settings.
- The asset image + `FLASH_IN_PROJECT` are left as-is (assets legitimately ship
  with firmware); only user data moved off that partition. CLAUDE.md still notes
  `app-flash` for fast iteration, but it's no longer required to protect wallets.
- Simulator: `IConfigStore` there is volatile (browser), so sim wallets don't
  persist across sim restarts — expected and unchanged.
