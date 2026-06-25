# Feature Reference

> How each user-facing feature works, how to use it, and how to extend it. This is the
> **feature/operator** view; for subsystem internals see [`../architecture/`](../architecture/README.md).
> Keep these current as features change (CLAUDE.md "Documentation — automated upkeep").

| Feature | Doc |
|---|---|
| BadUSB (Ducky-script keystroke injection) | [`badusb.md`](badusb.md) |
| Forge remote desktop (drive the device from a browser) | [`remote-desktop.md`](remote-desktop.md) |
| WASM simulator (firmware in the browser) | [`simulator.md`](simulator.md) |
| Firmware OTA updates | [`firmware-ota.md`](firmware-ota.md) |
| CLI terminal (multi-session shell) | [`cli-terminal.md`](cli-terminal.md) |
| File browser & storage (VFS) | [`file-browser.md`](file-browser.md) |
| Custom apps (JS `.papp` + SDK) | [`custom-apps.md`](custom-apps.md) |
| WiFi connectivity | [`wifi-connectivity.md`](wifi-connectivity.md) |
| Secure element (HW root-of-trust; crypto-wallet foundation) | [`secure-element.md`](secure-element.md) |
| Asset loader & animation pipeline | [`asset-loader.md`](asset-loader.md) |
| Dolphin showcase — system screen | [`dolphin-system-screen.md`](dolphin-system-screen.md) |
| Dolphin showcase — custom app | [`dolphin-custom-app.md`](dolphin-custom-app.md) |
| Mission Control (Flipper-style quick-settings panel) | [`mission-control.md`](mission-control.md) |
| Display rotation (0/90/180/270, input + touch follow) | [`display-rotation.md`](display-rotation.md) |
| Colour themes + dark mode (mono/Flipper palette) | [`colour-themes.md`](colour-themes.md) |
| Splash screen (boot + restart logo & progress bar) | [`splash-screen.md`](splash-screen.md) |

> Not yet documented as standalone features (covered in architecture docs): settings
> screens, lock/sleep, owner profile, audio (mic/speaker) and camera on SkyRizz E32, BLE
> scanner. Add a doc here when a feature graduates from prototype.
