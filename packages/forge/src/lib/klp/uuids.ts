// KLP GATT identifiers — shared contract between Forge (Web Bluetooth) and the
// device's BLE adapter (Plan 34/35). Custom 128-bit UUIDs under a Kairo base.
export const KLP_SERVICE = 'a7b30001-2c4f-4b9e-9c1a-6f0e2d3a4b5c';
export const KLP_CHAR_TX = 'a7b30002-2c4f-4b9e-9c1a-6f0e2d3a4b5c'; // device → host (notify)
export const KLP_CHAR_RX = 'a7b30003-2c4f-4b9e-9c1a-6f0e2d3a4b5c'; // host → device (write)
