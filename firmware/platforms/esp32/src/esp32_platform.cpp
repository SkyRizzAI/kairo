#include "kairo/esp32/esp32_platform.h"
#include "kairo/runtime.h"
#include "kairo/board.h"
#include "kairo/config/config_store.h"
#include "kairo/service/service_container.h"
#include "kairo/system/capability_registry.h"
#include "kairo/hal/display.h"
#include "kairo/log/logger.h"
#include "kairo/services/remote_service.h"
#include "kairo/plugins/js_app_store.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace kairo {

void Esp32Platform::registerDrivers(Runtime& rt) {
    rt_ = &rt;
    // Each driver self-registers via its lifecycle hook (deps, service, caps, hw).
    wifi_.onRegister(rt);
    http_.onRegister(rt);
    ble_.onRegister(rt);   // BLE radio (NimBLE) — capability "bluetooth.ble"

    // NVS-backed config store — available to all ESP32 boards.
    config_.init(rt.log());
    rt.container().registerService(&config_);
    rt.container().registerAs<IConfigStore>(&config_);
}

void Esp32Platform::postRegister(Runtime& rt) {
    // Wire the KLP remote layer (Plan 35/37) over a MUX of transports: USB-CDC
    // (always — the native USB) + BLE (if the radio is present). The host reaches
    // the device over whichever connects. Requires a display to mirror.
    if (!rt.capabilities().has("display")) return;
    auto* disp = rt.container().resolve<IDisplayDriver>();
    if (!disp) return;

    usbCdc_.start();                 // reader task on the USB-CDC (Serial)
    usbLink_.init(usbCdc_);
    mux_.add(&usbLink_);             // USB cable
    rt.capabilities().add("remote.usb");
    if (rt.capabilities().has("bluetooth.ble")) {
        cable_.init(ble_);          // KLP GATT TX/RX on the radio
        mux_.add(&cable_);          // BLE cable
    }
    link_.attach(&mux_, LinkService::Role::Device);
    tap_.init(*disp, link_);                            // decorate board display
    rt.container().registerAs<IDisplayDriver>(&tap_);   // Canvas now renders into tap

    remote_.init(link_, rt.input());                    // INPUT/SYSTEM dispatch
    remote_.attachLog(rt.log());                        // stream logs on LOG channel
    remote_.attachEvents(rt.events());                  // stream events on EVENT channel
    remote_.onPower(&Esp32Platform::powerThunk, this);
    remote_.onControl(&Esp32Platform::controlThunk, this);   // OTA app-install (Plan 37)
    remote_.setInfo(rt.board().name());
    link_.onReady(&Esp32Platform::readyThunk, this);    // push screen on connect

    remoteWired_ = true;
    rt.log().info("Esp32Platform", "KLP remote wired",
                  {{"usb", "1"}, {"ble", rt.capabilities().has("bluetooth.ble") ? "1" : "0"}});
}

void Esp32Platform::readyThunk(void* user) {
    static_cast<Esp32Platform*>(user)->tap_.requestResend();
}

void Esp32Platform::powerThunk(void* user, uint8_t op) {
    auto* s = static_cast<Esp32Platform*>(user);
    if (!s->rt_) return;
    if (op == SysOp::Restart)       s->rt_->requestRestart();
    else if (op == SysOp::Shutdown) s->rt_->requestShutdown();
}

void Esp32Platform::controlThunk(void* user, uint8_t op, const uint8_t* data, size_t len) {
    auto* s = static_cast<Esp32Platform*>(user);
    if (op == ExtOp::AppInstall && s->rt_)   // OTA: install a pushed .kapp live (Plan 37)
        JsAppStore::instance().installKapp(*s->rt_, (const char*)data, len);
}

void Esp32Platform::idle() {
    vTaskDelay(pdMS_TO_TICKS(5));
}

} // namespace kairo
