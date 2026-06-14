#pragma once

// PLP-over-BLE GATT contract (Plan 34/35). These 128-bit UUIDs are the shared
// wire identity between the device's BLE adapter (Esp32Ble) and any PLP host
// (Palanu Forge's Web Bluetooth client). Keep these in sync with
// packages/forge/src/lib/plp/uuids.ts — they MUST match byte-for-byte.
//
//   SERVICE  a7b30001-2c4f-4b9e-9c1a-6f0e2d3a4b5c   PLP transport service
//   TX char  a7b30002-2c4f-4b9e-9c1a-6f0e2d3a4b5c   device → host (Notify)
//   RX char  a7b30003-2c4f-4b9e-9c1a-6f0e2d3a4b5c   host → device (Write)
//
// The TX/RX naming is from the DEVICE's point of view: TX carries PLP frames
// the device emits (screen/log/event/…), RX carries frames the host sends
// (input/system/ext/…). Each BLE write/notify carries one PLP datagram; the
// PLP FrameParser reassembles the byte stream, so MTU need not align to frames.
namespace nema {
namespace plp_ble {

inline constexpr const char* SERVICE = "a7b30001-2c4f-4b9e-9c1a-6f0e2d3a4b5c";
inline constexpr const char* CHAR_TX = "a7b30002-2c4f-4b9e-9c1a-6f0e2d3a4b5c";
inline constexpr const char* CHAR_RX = "a7b30003-2c4f-4b9e-9c1a-6f0e2d3a4b5c";

} // namespace plp_ble
} // namespace nema
