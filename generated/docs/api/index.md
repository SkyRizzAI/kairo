# Nema System API Reference

> Auto-generated from [`api/*.pidl`](../../../api/). Do not edit.
> Version: `1.0`

## Interfaces

| Interface | Package | Capability | Functions |
|---|---|---|---|
| [`aether:ui/view`](./aether_ui_view.md) | `aether:ui@1.0` | `null` | 2 |
| [`aether:ui/text`](./aether_ui_text.md) | `aether:ui@1.0` | `null` | 2 |
| [`aether:ui/interactive`](./aether_ui_interactive.md) | `aether:ui@1.0` | `null` | 1 |
| [`aether:ui/scroll`](./aether_ui_scroll.md) | `aether:ui@1.0` | `null` | 2 |
| [`nema:bt/ble`](./nema_bt_ble.md) | `nema:bt@1.0` | `bt.ble` | 3 |
| [`nema:input/input`](./nema_input_input.md) | `nema:input@1.0` | `input` | 2 |
| [`nema:media/audio-input`](./nema_media_audio-input.md) | `nema:media@1.0` | `audio.input` | 1 |
| [`nema:media/audio-output`](./nema_media_audio-output.md) | `nema:media@1.0` | `audio.output` | 1 |
| [`nema:media/camera`](./nema_media_camera.md) | `nema:media@1.0` | `camera` | 2 |
| [`nema:net/http`](./nema_net_http.md) | `nema:net@1.0` | `net.http` | 2 |
| [`nema:net/wifi`](./nema_net_wifi.md) | `nema:net@1.0` | `net.wifi` | 6 |
| [`nema:profile/profile`](./nema_profile_profile.md) | `nema:profile@1.0` | `profile` | 4 |
| [`nema:storage/kv`](./nema_storage_kv.md) | `nema:storage@1.0` | core | 5 |
| [`nema:storage/fs`](./nema_storage_fs.md) | `nema:storage@1.0` | core | 5 |
| [`nema:sys/log`](./nema_sys_log.md) | `nema:sys@1.0` | core | 1 |
| [`nema:sys/device`](./nema_sys_device.md) | `nema:sys@1.0` | core | 4 |
| [`nema:sys/events`](./nema_sys_events.md) | `nema:sys@1.0` | core | 3 |
| [`nema:sys/perm`](./nema_sys_perm.md) | `nema:sys@1.0` | core | 2 |
| [`nema:sys/lease`](./nema_sys_lease.md) | `nema:sys@1.0` | core | 2 |
| [`nema:sys/tasks`](./nema_sys_tasks.md) | `nema:sys@1.0` | core | 4 |
| [`nema:wifi/radio`](./nema_wifi_radio.md) | `nema:wifi@1.0` | core | 5 |

## Records

| Record | Package | Fields |
|---|---|---|
| `http-response` | `nema:net` | `status: u16`, `body: string` |
| `wifi-ap` | `nema:net` | `ssid: string`, `rssi: s32`, `auth: string` |
| `field` | `nema:sys` | `key: string`, `value: string` |
| `lease-error` | `nema:sys` | `code: string`, `owner: string` |
| `scan-result` | `nema:wifi` | `bssid: string`, `ssid: string`, `channel: u8`, `rssi: s32`, `auth: string` |