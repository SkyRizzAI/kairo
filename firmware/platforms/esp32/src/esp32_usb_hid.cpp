#include "nema/esp32/esp32_usb_hid.h"
#include "nema/system/capabilities.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/service/service_container.h"
#include "nema/system/capability_registry.h"
#include "nema/system/hardware_registry.h"

// Plan 66 — USB HID keyboard (BadUSB).
// Guarded by ARDUINO_USB_ON_BOOT: in JTAG/Serial mode (flag absent) TinyUSB is
// never initialized, so the global USBHIDKeyboard constructor and the preloader
// must NOT run — they would interfere with the built-in USB Serial/JTAG hardware
// block and break USB-Serial remote desktop even without a flashed USB-CDC port.
// Preloader (priority 201) pre-calls tud_hid_descriptor_report_cb() in main_task
// context (CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192) so the lazy esp_hid_parse_report_map()
// runs there instead of in usb_device_task (4KB stack), preventing stack overflow.
// Descriptor malloc uses PSRAM (CONFIG_SPIRAM=y) so it never OOMs.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifdef ARDUINO_USB_ON_BOOT
#include "USBHIDKeyboard.h"
extern "C" uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance);
static USBHIDKeyboard s_keyboard __attribute__((init_priority(200)));
struct Esp32HidPreloader {
    Esp32HidPreloader() { tud_hid_descriptor_report_cb(0); }
};
static Esp32HidPreloader s_hid_preload __attribute__((init_priority(201)));
#endif // ARDUINO_USB_ON_BOOT

namespace nema {

void Esp32UsbHid::onRegister(Runtime& rt) {
    rt.container().registerService(this);
    rt.container().registerAs<IUsbHid>(this);
    rt.hardware().add({"usb.hid", DriverKind::Other, "USB HID Keyboard"});
    rt.capabilities().add(caps::UsbHid);
#ifdef ARDUINO_USB_ON_BOOT
    s_keyboard.begin();
    ready_ = true;
#endif
}

void Esp32UsbHid::start() {}

void Esp32UsbHid::stop() {
    ready_ = false;
}

void Esp32UsbHid::sendKey(uint8_t modifier, uint8_t keycode) {
#ifdef ARDUINO_USB_ON_BOOT
    if (!ready_) return;
    // modifier and keycode use Arduino key constants (0x80–0x87 = modifier keys,
    // 0x88–0xFF = special keys, 0x20–0x7E = ASCII). press() translates these to
    // the correct HID report bits internally — do NOT use sendReport() directly.
    if (modifier) s_keyboard.press(modifier);
    if (keycode)  s_keyboard.press(keycode);
    vTaskDelay(pdMS_TO_TICKS(10));
    s_keyboard.releaseAll();
    vTaskDelay(pdMS_TO_TICKS(5));
#else
    (void)modifier; (void)keycode;
#endif
}

void Esp32UsbHid::sendString(const char* s, uint32_t delayMs) {
#ifdef ARDUINO_USB_ON_BOOT
    if (!ready_ || !s) return;
    const uint32_t perChar = (delayMs > 0) ? delayMs : 5;
    while (*s) {
        s_keyboard.write(static_cast<uint8_t>(*s++));
        vTaskDelay(pdMS_TO_TICKS(perChar));
    }
#else
    (void)s; (void)delayMs;
#endif
}

void Esp32UsbHid::delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void Esp32UsbHid::releaseAll() {
#ifdef ARDUINO_USB_ON_BOOT
    if (!ready_) return;
    s_keyboard.releaseAll();
#endif
}

} // namespace nema
