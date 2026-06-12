# Palanu Master Plan v0.5

> Single source of truth untuk seluruh perencanaan Palanu Runtime, Palanu Simulator, dan Palanu Board.

---

# 1. Vision

Palanu adalah platform handheld device yang berfokus pada:

- Hardware hacking
- Wireless experimentation
- Plugin ecosystem
- Developer experience
- Extensible runtime architecture
- Simulator-first development

Targetnya bukan sekadar membuat perangkat mirip Flipper Zero.

Tujuan jangka panjang adalah membangun sebuah platform yang memiliki:

- Runtime yang portable
- Plugin system yang kuat
- Hardware abstraction yang baik
- Simulator untuk development cepat
- Dukungan multi-device di masa depan

---

# 2. Official Naming

## Runtime

Palanu Runtime

Core operating/runtime layer yang berjalan pada seluruh perangkat Palanu.

---

## SDK

Palanu SDK

Framework untuk mengembangkan:

- Apps
- Plugins
- Services

---

## Simulator

Palanu Simulator

Environment development dan debugging berbasis web.

---

## Hardware

Palanu Board

Nama resmi hardware Palanu.

Contoh:

- Palanu Board DevKit S3
- Palanu Board V1
- Palanu Board V2

---

# 3. Core Principles

## Simulator First

Semua fitur sebisa mungkin dapat dikembangkan dan diuji tanpa hardware.

---

## Observability First

Debugging adalah prioritas.

Runtime harus mudah diinspeksi.

---

## Plugin First

Fitur tambahan harus dapat dikembangkan sebagai plugin.

---

## Hardware Agnostic

Core Runtime tidak boleh mengetahui hardware tertentu.

---

## Capability Driven

Aplikasi tidak memeriksa jenis hardware.

Aplikasi memeriksa capability.

Contoh:

```cpp
if (capabilities.has("wifi"))
```

Bukan:

```cpp
if (isEsp32())
```

---

# 4. Repository Structure

```text
palanu/

firmware/
├─ core/
├─ platforms/
├─ boards/
├─ targets/
└─ tools/

packages/
└─ simulator-web/

docs/

package.json
bun.lock
```

---

# 5. Architecture Hierarchy

```text
Core
 ↓
Platform
 ↓
Board
 ↓
Target
```

---

# 6. Core Runtime

Core Runtime adalah bagian terpenting dari sistem.

Core tidak mengetahui:

- ESP32
- Display tertentu
- WiFi tertentu
- Board tertentu

Core hanya mengenal interface.

---

# 7. Core Services

## Logger Service

Service pertama yang harus tersedia.

Semua subsystem wajib menggunakan logger.

Tidak boleh menggunakan:

```cpp
printf()
```

secara langsung.

---

### Log Levels

- TRACE
- DEBUG
- INFO
- WARN
- ERROR
- FATAL

---

### Log Entry

Contoh:

```text
[12:00:01] [INFO ] [Runtime] Booting

[12:00:02] [INFO ] [Logger] Initialized

[12:00:03] [ERROR] [Wifi] Connection Failed
```

---

### Structured Logging

Contoh:

```json
{
  "level": "ERROR",
  "component": "WifiService",
  "message": "Connection Failed",
  "ssid": "Office"
}
```

---

### Log Sinks

- Console Sink
- Memory Sink
- File Sink
- Remote Sink (future)

---

## Event Bus

Backbone komunikasi internal.

---

### Example Events

- SystemBoot
- SystemReady
- ServiceStarted
- ServiceStopped
- PluginLoaded
- PluginUnloaded
- NotificationCreated
- BatteryChanged
- NetworkConnected
- NetworkDisconnected

---

## Service Container

Dependency Injection Container.

Responsibilities:

- Register Service
- Resolve Service
- Singleton Management
- Lifecycle Management

---

## Service Manager

Mengelola lifecycle service.

States:

- Created
- Starting
- Running
- Stopping
- Stopped
- Failed

---

## Plugin Manager

Mengelola:

- Loading
- Unloading
- Lifecycle
- Permissions
- Registration

---

## Notification Manager

Mengelola notifikasi sistem.

---

## Network Manager

Abstraksi seluruh konektivitas jaringan.

Aplikasi tidak berbicara langsung ke driver.

Flow:

```text
App
 ↓
Network Manager
 ↓
Network Adapter
 ↓
Driver
```

---

## Display Manager

Abstraksi display.

Aplikasi tidak berbicara langsung ke display driver.

---

## Storage Manager

Abstraksi storage.

Mendukung:

- Internal Flash
- SD Card
- Future Storage Providers

---

# 8. System Introspection

## SystemInfo

Memberikan informasi runtime.

Contoh:

- CPU
- RAM
- PSRAM
- Flash
- Build Version
- Firmware Version
- Board Version

---

## Hardware Registry

Mendaftarkan hardware yang tersedia.

Contoh:

- Display
- WiFi
- Bluetooth
- Battery
- Speaker
- Microphone
- NFC
- RFID
- Infrared
- SubGHz
- GPS
- SD Card

---

## Capability Registry

Mendaftarkan kemampuan yang tersedia.

Contoh:

- Networking
- Notifications
- OTA
- Plugin Runtime
- Script Runtime
- Background Services

---

# 9. Platform Layer

Platform adalah implementasi runtime environment.

---

## Simulator Platform

Digunakan untuk simulator.

Menyediakan:

- Mock Wifi
- Mock Bluetooth
- Mock Display
- Mock Storage
- Mock Battery

---

## ESP32 Platform

Digunakan untuk perangkat nyata.

Menyediakan:

- ESP Wifi
- ESP Bluetooth
- ESP Display
- ESP Storage
- ESP Battery

---

# 10. Board Layer

Board mendefinisikan hardware.

---

## Simulator Board

Virtual hardware.

---

## DevKit S3 Board

Development board.

Current hardware:

ESP32-S3-WROOM-1-N8R8

Purpose:

- Runtime Development
- Driver Development
- Plugin Testing
- Integration Testing

---

## Palanu Board V1

Production hardware target.

---

# 11. Target Layer

Target adalah entry point firmware.

---

## Simulator Target

```text
Target
  simulator

Board
  simulator

Platform
  simulator
```

---

## DevKit Target

```text
Target
  devkit-s3

Board
  devkit-s3

Platform
  esp32
```

---

## Palanu Board V1 Target

```text
Target
  palanu-board-v1

Board
  palanu-board-v1

Platform
  esp32
```

---

# 12. Boot Flow

```text
main()

 ↓

Create Runtime

 ↓

Load Platform

 ↓

Load Board

 ↓

Register Services

 ↓

Initialize Core

 ↓

Start Runtime

 ↓

Launch Home Screen
```

---

# 13. UI Architecture

```text
Plugin
 ↓
Screen
 ↓
UI Runtime
 ↓
Display Manager
 ↓
Display Adapter
 ↓
Display Driver
 ↓
Hardware
```

---

# 14. UI Runtime

Standar UI resmi Palanu.

Plugin tidak boleh menggambar pixel secara langsung.

---

## Standard Components

- Screen
- Label
- Button
- List
- Menu
- Dialog
- Notification
- StatusBar
- ProgressBar
- Icon

---

# 15. Screen System

Plugin membuat screen.

Plugin tidak menggambar display.

Flow:

```text
Plugin
 ↓
Screen
 ↓
UI Runtime
 ↓
Display Manager
 ↓
Display Driver
```

---

# 16. Status Bar

Core Status Items:

- WiFi
- Bluetooth
- Battery
- Clock

---

Plugin Status Items:

Plugin dapat menambahkan icon ke status bar.

Contoh:

- VPN
- Script Running
- Background Service

---

# 17. Background Services

Foreground:

Hanya satu aplikasi aktif.

Background:

Banyak service dapat berjalan bersamaan.

Contoh:

- Clock Service
- Notification Service
- Wifi Service
- Bluetooth Service

---

# 18. Simulator

Simulator bukan emulator hardware.

Simulator menjalankan:

- Palanu Runtime
- Plugin Runtime
- UI Runtime
- Mock Drivers

---

# 19. Palanu Simulator

Location:

```text
packages/simulator-web
```

---

Technology:

- Bun
- TypeScript
- React
- Vite
- WebSocket

---

# 20. Simulator Panels

## Display

Render virtual display.

---

## Logs

Realtime logs.

Filter:

- TRACE
- DEBUG
- INFO
- WARN
- ERROR
- FATAL

---

## Events

Realtime event stream.

---

## Services

Realtime service lifecycle monitor.

---

## Hardware Registry

Hardware visibility.

---

## Capability Registry

Capability visibility.

---

## Controls

Actions:

- Boot
- Shutdown
- Restart
- Factory Reset
- Install Plugin
- Remove Plugin
- Toggle WiFi
- Toggle Bluetooth
- Inject Event
- Change Battery Level

---

# 21. Palanu Board V1

## Goal

Minimal capability target harus setara dengan Flipper Zero.

Dengan runtime dan plugin ecosystem yang lebih modern.

---

# 22. Palanu Board V1 Hardware Targets

## Processing

Candidate:

- ESP32-S3
- ESP32-P4 + Companion Radio MCU

Decision TBD.

---

## Memory

Minimum Target:

Flash:

```text
32 MB
```

PSRAM:

```text
8 MB
```

Storage:

```text
MicroSD
```

---

# 23. Connectivity

## WiFi

Required

Use Cases:

- OTA
- Plugin Download
- Cloud Features
- Remote Control

---

## Bluetooth

Required

Use Cases:

- Mobile Companion
- BLE Tools
- Device Pairing

---

# 24. Radio Features

## SubGHz

Required

Target Bands:

- 315 MHz
- 433 MHz
- 868 MHz
- 915 MHz

---

## NFC

Required

Target:

13.56 MHz

---

## RFID

Required

Target:

125 kHz

---

## Infrared

Required

Features:

- RX
- TX

---

# 25. Audio

## Speaker

Required

---

## Microphone

Required

---

# 26. Storage

## MicroSD

Required

Use Cases:

- Plugins
- Scripts
- Logs
- Media
- Backups

---

# 27. Power

Required:

- Rechargeable Battery
- USB-C Charging
- Battery Monitoring
- Battery Protection

---

# 28. Input

Required:

- Navigation Buttons
- Action Button
- Back Button
- Power Button

Layout TBD.

---

# 29. Display

Target:

Color IPS Display

Requirements:

- Good readability
- Low power consumption
- Fast refresh rate

---

# 30. Expansion

Reserved expansion capability.

Potential future modules:

- GPS
- LoRa
- External Radios
- Custom Add-ons

---

# 31. Mandatory Capabilities

Palanu Board V1 wajib memiliki:

- WiFi
- Bluetooth
- NFC
- RFID
- Infrared
- SubGHz
- Speaker
- Microphone
- MicroSD
- USB-C
- Battery Monitoring
- Plugin Runtime
- OTA Updates
- Background Services

---

# 32. Development Roadmap

## Milestone 1

Core Foundation

- Logger
- Event Bus
- Service Container
- Boot Sequence

---

## Milestone 2

Observability

- Log Viewer
- Event Viewer
- Service Inspector

---

## Milestone 3

Simulator

- Simulator Platform
- Simulator Board
- Simulator Web

---

## Milestone 4

Plugin Runtime

- Plugin Manager
- Plugin Lifecycle
- Plugin API

---

## Milestone 5

UI Runtime

- Status Bar
- Screen System
- Home Screen

---

## Milestone 6

ESP32 Dev Hardware

Target:

DevKit S3

Board:

ESP32-S3-WROOM-1-N8R8

---

## Milestone 7

Palanu Board V1

- Hardware Design
- PCB Design
- Prototype
- Validation

---

# Current Development Hardware

Board:

Palanu Board DevKit S3

MCU:

ESP32-S3-WROOM-1-N8R8

Flash:

8 MB

PSRAM:

8 MB

Purpose:

- Runtime Development
- Driver Testing
- Integration Testing
- Simulator Validation

Before Palanu Board V1 exists.
