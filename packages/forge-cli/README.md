# @palanu/forge-cli

Forge CLI — remote device management for Palanu. Manage multiple devices, remote shell, and file copy over USB serial or WebSocket network.

## Quickstart

```bash
# Register a device
palanu add mydev serial:///dev/cu.usbmodem123
palanu add skyrizz ws://skyrizz.local:8477/plp

# List devices
palanu list

# Connect
palanu connect mydev

# Remote shell (like SSH)
palanu shell mydev
# / $ hwinfo
# / $ ps
# / $ exit

# Copy file (like scp)
palanu cp device:mydev:/system/apps/app.papp ./backup/    # pull
palanu cp ./myapp.papp.zip device:mydev:/system/apps/     # push

# Disconnect
palanu disconnect mydev

# Remove from registry
palanu remove mydev
```

## Commands

| Command | Description |
|---|---|
| `add <name> <target>` | Register a device (alias → target URL) |
| `list` | List registered devices with connection status |
| `connect <name>` | Connect to a device by alias |
| `disconnect <name>` | Disconnect from a device |
| `remove <name>` | Remove a device from the registry |
| `shell <name>` | Interactive remote CLI (REPL) — like SSH |
| `cp <src> <dst>` | Copy file device↔local (like scp) |

## Target URL formats

| Scheme | Example | Transport |
|---|---|---|
| `serial://` | `serial:///dev/cu.usbmodem123` | USB serial (serialport) |
| `ws://` | `ws://host:8477/plp` | WebSocket network |
| `wss://` | `wss://host:8477/plp` | WebSocket Secure |

BLE transport (`ble://`) is planned but not yet implemented.

## Path format for `cp`

- **Remote**: `device:<alias>:/path/to/file` — file on the device
- **Local**: `./path/to/file` or `/absolute/path` — file on your machine

```bash
# Pull: device → local
palanu cp device:mydev:/path/to/file ./local/

# Push: local → device
palanu cp ./local/file device:mydev:/path/to/
```

## Configuration

Device registry and auth tokens are stored in `~/.palanu/config.json`:

```json
{
  "devices": {
    "mydev": {
      "target": "serial:///dev/cu.usbmodem123",
      "token": null,
      "lastConnected": "2026-06-21T12:00:00.000Z"
    }
  }
}
```

## Architecture

```
@palanu/forge-cli
  ├── @palanu/link        (protocol: PLP codec + RemoteSession + ILinkTransport)
  ├── commander            (arg parser)
  ├── serialport           (USB serial transport)
  └── ws                   (WebSocket transport)
```

All protocol logic (PLP codec, RemoteSession, channel handlers) is shared with Forge web via `@palanu/link`. The CLI only adds Node.js-specific transports and a command-line interface.
