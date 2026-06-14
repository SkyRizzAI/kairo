#include "nema/system/capabilities.h"
#include "nema/esp32/esp32_ble.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/event/async_event_poster.h"
#include "nema/service/service_container.h"
#include "nema/system/hardware_registry.h"
#include "nema/system/capability_registry.h"
#include "nema/link/plp_ble.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace nema {

void Esp32Ble::onRegister(Runtime& rt) {
    log_    = &rt.log();
    events_ = &rt.events();
    poster_ = &rt.asyncPoster();
    rt.container().registerService(this);
    rt.container().registerAs<IBluetoothController>(this);
    rt.container().registerAs<IBleAdapter>(this);
    rt.hardware().add({"bluetooth", DriverKind::Bluetooth, "ESP32-S3 BLE (NimBLE)"});
    rt.capabilities().add(caps::BtBle);
}

} // namespace nema

// ─────────────────────────────────────────────────────────────────────────────
#ifdef CONFIG_BT_NIMBLE_ENABLED

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

extern "C" void ble_store_config_init(void);

namespace nema {

static Esp32Ble* g_ble = nullptr;

// ── PLP GATT service (see core/.../link/plp_ble.h) ──
// NimBLE 128-bit UUIDs are little-endian: reverse of the canonical byte order.
//   SERVICE a7b30001-2c4f-4b9e-9c1a-6f0e2d3a4b5c
//   TX      a7b30002-…  (Notify, device→host)
//   RX      a7b30003-…  (Write,  host→device)
static const ble_uuid128_t PLP_SVC_UUID = BLE_UUID128_INIT(
    0x5c, 0x4b, 0x3a, 0x2d, 0x0e, 0x6f, 0x1a, 0x9c,
    0x9e, 0x4b, 0x4f, 0x2c, 0x01, 0x00, 0xb3, 0xa7);
static const ble_uuid128_t PLP_TX_UUID = BLE_UUID128_INIT(
    0x5c, 0x4b, 0x3a, 0x2d, 0x0e, 0x6f, 0x1a, 0x9c,
    0x9e, 0x4b, 0x4f, 0x2c, 0x02, 0x00, 0xb3, 0xa7);
static const ble_uuid128_t PLP_RX_UUID = BLE_UUID128_INIT(
    0x5c, 0x4b, 0x3a, 0x2d, 0x0e, 0x6f, 0x1a, 0x9c,
    0x9e, 0x4b, 0x4f, 0x2c, 0x03, 0x00, 0xb3, 0xa7);

static uint16_t g_plp_tx_handle = 0;   // notify val handle, set by ble_gatts_add_svcs

static int plp_gatt_access(uint16_t conn, uint16_t attr,
                           struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn; (void)attr; (void)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t  buf[256];
        uint16_t out = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &out);
        if (rc == 0 && g_ble) g_ble->onRxWrite(buf, out);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_chr_def PLP_CHRS[] = {
    { .uuid = &PLP_TX_UUID.u, .access_cb = plp_gatt_access, .arg = nullptr,
      .descriptors = nullptr, .flags = BLE_GATT_CHR_F_NOTIFY,
      .min_key_size = 0, .val_handle = &g_plp_tx_handle },
    { .uuid = &PLP_RX_UUID.u, .access_cb = plp_gatt_access, .arg = nullptr,
      .descriptors = nullptr, .flags = BLE_GATT_CHR_F_WRITE,
      .min_key_size = 0, .val_handle = nullptr },
    { 0 },
};
static const struct ble_gatt_svc_def PLP_SVCS[] = {
    { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &PLP_SVC_UUID.u,
      .includes = nullptr, .characteristics = PLP_CHRS },
    { 0 },
};

static int gap_event_thunk(struct ble_gap_event* event, void* arg) {
    (void)arg;
    return g_ble ? g_ble->onGapEvent(event) : 0;
}
static void on_sync_thunk() {
    if (g_ble) g_ble->onSync();
}
static void host_task(void*) {
    nimble_port_run();              // returns only on nimble_port_stop()
    nimble_port_freertos_deinit();
}

bool Esp32Ble::enable(BtMode mode) {
    if (mode == BtMode::Classic || mode == BtMode::Dual) {
        if (log_) log_->warn("Esp32Ble", "Classic BT unsupported (ESP32-S3 = BLE only)");
        return false;
    }
    if (enabled_) return true;
    g_ble = this;

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        if (log_) log_->error("Esp32Ble", "nimble_port_init failed");
        return false;
    }

    // Security: LE Secure Connections + MITM (numeric comparison) + bonding.
    ble_hs_cfg.sync_cb         = on_sync_thunk;
    ble_hs_cfg.sm_io_cap       = BLE_HS_IO_DISPLAY_YESNO;
    ble_hs_cfg.sm_sc           = 1;
    ble_hs_cfg.sm_mitm         = 1;
    ble_hs_cfg.sm_bonding      = 1;
    ble_hs_cfg.sm_our_key_dist  = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.store_status_cb  = ble_store_util_status_rr;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Register the PLP transport GATT service (Plan 35). Must run before the host
    // syncs / advertising starts so the service is committed to the GATT table.
    int grc = ble_gatts_count_cfg(PLP_SVCS);
    if (grc == 0) grc = ble_gatts_add_svcs(PLP_SVCS);
    if (grc != 0 && log_) log_->error("Esp32Ble", "PLP GATT register failed",
                                      {{"rc", std::to_string(grc)}});

    ble_svc_gap_device_name_set(devName_.c_str());
    ble_store_config_init();

    nimble_port_freertos_init(host_task);

    enabled_ = true;
    mode_    = BtMode::Ble;
    if (events_ && poster_) poster_->post({events::BtEnabled, {}});
    if (log_) log_->info("Esp32Ble", "enabled (NimBLE)");
    return true;
}

void Esp32Ble::disable() {
    if (!enabled_) return;
    nimble_port_stop();
    nimble_port_deinit();
    enabled_ = false;
    mode_ = BtMode::Off;
    advertising_ = false;
    if (poster_) poster_->post({events::BtDisabled, {}});
}

void Esp32Ble::onSync() {
    uint8_t addr_val[6] = {0};
    int rc = ble_hs_id_infer_auto(0, &addr_val[0]);  // sets own addr type byte
    (void)rc;
    uint8_t mac[6] = {0};
    ble_hs_id_copy_addr(BLE_OWN_ADDR_PUBLIC, mac, nullptr);
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
    addr_ = buf;
    startAdvertisingInternal();
}

void Esp32Ble::startAdvertisingInternal() {
    struct ble_gap_adv_params adv_params{};
    struct ble_hs_adv_fields fields{};

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t*)devName_.c_str();
    fields.name_len = (uint8_t)devName_.size();
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    uint8_t own_addr_type = BLE_OWN_ADDR_PUBLIC;
    ble_hs_id_infer_auto(0, &own_addr_type);
    int rc = ble_gap_adv_start(own_addr_type, nullptr, BLE_HS_FOREVER,
                               &adv_params, gap_event_thunk, nullptr);
    advertising_ = (rc == 0);
    if (log_) log_->info("Esp32Ble", advertising_ ? "advertising" : "advertising failed");
}

bool Esp32Ble::startAdvertising() {
    if (!enabled_) return false;
    startAdvertisingInternal();
    return advertising_;
}

void Esp32Ble::stopAdvertising() {
    ble_gap_adv_stop();
    advertising_ = false;
}

int Esp32Ble::onGapEvent(void* ev) {
    auto* event = static_cast<struct ble_gap_event*>(ev);
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                connHandle_ = event->connect.conn_handle;
                struct ble_gap_conn_desc desc{};
                if (ble_gap_conn_find(connHandle_, &desc) == 0) {
                    std::memcpy(peer_.addr, desc.peer_id_addr.val, 6);
                }
                std::snprintf(peer_.name, sizeof(peer_.name), "peer");
                advertising_ = false;
                if (poster_) poster_->post({events::BtConnected, {{"name", peer_.name}}});
            } else {
                startAdvertisingInternal();   // failed → keep advertising
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            connHandle_ = 0xFFFF;
            peer_ = BtPeer{};
            if (poster_) poster_->post({events::BtDisconnected, {}});
            startAdvertisingInternal();
            break;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
                pendingPasskey_ = event->passkey.params.numcmp;
                pairing_ = true;
                if (poster_)
                    poster_->post({events::BtPairRequest,
                                   {{"passkey", std::to_string(pendingPasskey_)}}});
            }
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                peer_.bonded = true;
                if (poster_) poster_->post({events::BtPaired, {{"name", peer_.name}}});
            }
            break;
        default:
            break;
    }
    return 0;
}

void Esp32Ble::confirmPairing(bool accept) {
    if (!pairing_) return;
    pairing_ = false;
    struct ble_sm_io io{};
    io.action = BLE_SM_IOACT_NUMCMP;
    io.numcmp_accept = accept ? 1 : 0;
    if (connHandle_ != 0xFFFF) ble_sm_inject_io(connHandle_, &io);
}

bool Esp32Ble::peer(BtPeer& out) const {
    if (connHandle_ == 0xFFFF) return false;
    out = peer_;
    return true;
}

void Esp32Ble::disconnect() {
    if (connHandle_ != 0xFFFF) ble_gap_terminate(connHandle_, BLE_ERR_REM_USER_CONN_TERM);
}

bool Esp32Ble::notify(const char* charUuid, const uint8_t* data, size_t len) {
    if (connHandle_ == 0xFFFF) return false;
    // Only the PLP TX characteristic is notify-capable here.
    if (std::strcmp(charUuid, plp_ble::CHAR_TX) != 0 || g_plp_tx_handle == 0) return false;
    struct os_mbuf* om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return false;                       // mbuf pool exhausted → drop frame
    int rc = ble_gatts_notify_custom(connHandle_, g_plp_tx_handle, om);
    return rc == 0;
}

void Esp32Ble::onRxWrite(const uint8_t* data, size_t len) {
    // Called from the NimBLE host task; deliver to the BleLinkTransport's RecvFn.
    if (writeFn_) writeFn_(writeUser_, plp_ble::CHAR_RX, data, len);
}

void Esp32Ble::registerService(const BleService& svc) {
    // The PLP service is built in statically (PLP_SVCS) and committed in enable();
    // this hook records the request for diagnostics. Generic app-defined GATT
    // services are a future extension.
    if (log_) log_->info("Esp32Ble", "GATT service registered", {{"uuid", svc.uuid}});
}

size_t Esp32Ble::bondedCount() const {
    int count = 0;
    ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &count);
    return (size_t)count;
}

bool Esp32Ble::bondedAt(size_t i, BtPeer& out) const {
    ble_addr_t addrs[8];
    int num = 0;
    if (ble_store_util_bonded_peers(addrs, &num, 8) != 0) return false;
    if ((int)i >= num) return false;
    std::memcpy(out.addr, addrs[i].val, 6);
    out.bonded = true;
    std::snprintf(out.name, sizeof(out.name), "bond%zu", i);
    return true;
}

void Esp32Ble::forget(const uint8_t addr[6]) {
    ble_addr_t a{};
    a.type = BLE_ADDR_PUBLIC;
    std::memcpy(a.val, addr, 6);
    ble_gap_unpair(&a);
}

void Esp32Ble::forgetAll() {
    ble_addr_t addrs[8];
    int num = 0;
    if (ble_store_util_bonded_peers(addrs, &num, 8) != 0) return;
    for (int i = 0; i < num; i++) ble_gap_unpair(&addrs[i]);
}

} // namespace nema

#else  // ── BT disabled: no-op stubs so non-BT boards still link ──
namespace nema {
bool Esp32Ble::enable(BtMode) { if (log_) log_->warn("Esp32Ble", "BT not enabled in sdkconfig"); return false; }
void Esp32Ble::disable() {}
void Esp32Ble::onSync() {}
void Esp32Ble::startAdvertisingInternal() {}
bool Esp32Ble::startAdvertising() { return false; }
void Esp32Ble::stopAdvertising() {}
int  Esp32Ble::onGapEvent(void*) { return 0; }
void Esp32Ble::confirmPairing(bool) {}
bool Esp32Ble::peer(BtPeer&) const { return false; }
void Esp32Ble::disconnect() {}
bool Esp32Ble::notify(const char*, const uint8_t*, size_t) { return false; }
void Esp32Ble::onRxWrite(const uint8_t*, size_t) {}
void Esp32Ble::registerService(const BleService&) {}
size_t Esp32Ble::bondedCount() const { return 0; }
bool Esp32Ble::bondedAt(size_t, BtPeer&) const { return false; }
void Esp32Ble::forget(const uint8_t[6]) {}
void Esp32Ble::forgetAll() {}
} // namespace nema
#endif
