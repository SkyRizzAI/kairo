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
    rt.capabilities().add(caps::BtBleCentral);
    caps_ = &rt.capabilities();
}

// In-flight TX notifications (0 in the no-BT stub build, where notify() never runs).
int Esp32Ble::txPending() const { return pendingNotify_.load(std::memory_order_relaxed); }

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
#include "esp_heap_caps.h"

extern "C" void ble_store_config_init(void);

// ⚠️ THE root-cause fix (Plan 93). arduino-esp32's initArduino() calls
// esp_bt_controller_mem_release(ESP_BT_MODE_BTDM) at boot — freeing the BT controller's
// memory — UNLESS btInUse() returns true. btInUse() is a WEAK symbol defaulting to false,
// and arduino explicitly invites a strong user override (esp32-hal-bt.c). Without this,
// our later esp_bt_controller_init() runs on released memory and crashes deterministically
// inside btdm_controller_init (the entire boot-loop saga). We DO use BT → claim it.
extern "C" bool btInUse(void) { return true; }

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
      // WRITE_NO_RSP is REQUIRED: Forge sends via writeValueWithoutResponse() (low
      // latency, no round-trip). Without this flag that JS call rejects silently and
      // the PLP HELLO never reaches the device → Forge stuck "connecting" (Plan 93).
      .descriptors = nullptr, .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
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

// One-time controller + NimBLE host bring-up. Called at BOOT (start()) while the
// heap still has a big contiguous internal block — NOT on the user toggle, which
// hits a fragmented heap and crashes esp_bt_controller_init (Plan 93).
bool Esp32Ble::initStack() {
    if (stackUp_) return true;

    // ── Internal-RAM pre-flight (Plan 93) ────────────────────────────────────
    // The BLE controller (esp_bt_controller_init) needs contiguous INTERNAL DRAM
    // for its ISR/DMA structures — it cannot run from PSRAM. On this RAM-tight board
    // (WiFi + LVGL + camera + audio all share ~230 KB internal) it can be exhausted
    // by the time the user toggles BLE on. If the controller's init then fails, its
    // own rollback (btdm_controller_deinit_internal → semphr_delete_wrapper) faults
    // on an uninitialised handle and PANICS THE WHOLE DEVICE (LoadProhibited). So we
    // measure first and refuse gracefully instead of letting it crash.
    size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (log_) log_->info("Esp32Ble", "pre-init internal RAM",
                         {{"free", std::to_string(freeInternal)},
                          {"largest", std::to_string(largestBlock)}});
    // Floors are a graceful-OOM net only (the real crash was btInUse — ADR 0013 — not
    // RAM; that's fixed). The controller's host/LL pools live in PSRAM (NimBLE EXTERNAL +
    // SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY, mirroring Flipper-ESP32), so its remaining
    // INTERNAL need is modest — a couple of task stacks. Keep a low floor so BLE can come
    // up alongside WiFi+apps, but still refuse (Bluetooth "unavailable") rather than risk
    // a genuine alloc failure inside the blob if internal RAM is truly exhausted.
    constexpr size_t kFreeFloor  = 24 * 1024;   // total internal headroom
    constexpr size_t kBlockFloor = 10 * 1024;   // largest contiguous chunk
    if (freeInternal < kFreeFloor || largestBlock < kBlockFloor) {
        if (log_) log_->error("Esp32Ble", "insufficient contiguous internal RAM for BLE",
                              {{"free", std::to_string(freeInternal)},
                               {"largest", std::to_string(largestBlock)},
                               {"needFree", std::to_string(kFreeFloor)},
                               {"needBlock", std::to_string(kBlockFloor)}});
        if (caps_) caps_->setState(caps::BtBle, ResourceState::Fault);
        return false;
    }

    g_ble = this;

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        if (log_) log_->error("Esp32Ble", "nimble_port_init failed");
        if (caps_) caps_->setState(caps::BtBle, ResourceState::Fault);
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

    stackUp_ = true;
    if (caps_) caps_->setState(caps::BtBle, ResourceState::Available);
    if (log_) log_->info("Esp32Ble", "BLE stack up (controller reserved at boot)");
    return true;
}

// Boot hook (IService::start). The BLE controller is brought up ON DEMAND (first
// enable()), NOT here — on this RAM-tight board the controller's ~30 KB working memory
// (allocated at init) starves the SD-card DMA buffer when taken at boot, so SD reads
// fail and apps loaded from /sd get dropped. Deferring keeps boot clean; the btInUse()
// override above is what lets on-demand init succeed (no arduino mem-release crash).
// (Fragmentation was never the issue — that was a wrong early theory; see Plan 93.)
void Esp32Ble::start() {}

// User toggle ON. Controller is already up (boot); just begin advertising. Light &
// crash-free — no nimble_port_init here. (Caller then calls startAdvertising().)
bool Esp32Ble::enable(BtMode mode) {
    if (mode == BtMode::Classic || mode == BtMode::Dual) {
        if (log_) log_->warn("Esp32Ble", "Classic BT unsupported (ESP32-S3 = BLE only)");
        return false;
    }
    if (!stackUp_ && !initStack()) return false;   // boot init failed (RAM) → unavailable
    if (enabled_) return true;
    enabled_ = true;
    mode_    = BtMode::Ble;
    if (events_ && poster_) poster_->post({events::BtEnabled, {}});
    if (log_) log_->info("Esp32Ble", "enabled (advertising)");
    return true;
}

void Esp32Ble::disable() {
    if (!enabled_) return;
    // Keep the controller/host UP (reserved at boot). Only stop advertising and drop
    // the link — re-init/deinit on a fragmented heap is exactly what crashed before.
    stopAdvertising();
    if (connHandle_ != 0xFFFF) ble_gap_terminate(connHandle_, BLE_ERR_REM_USER_CONN_TERM);
    enabled_ = false;
    mode_    = BtMode::Off;
    if (poster_) poster_->post({events::BtDisabled, {}});
    if (log_) log_->info("Esp32Ble", "disabled (advertising off)");
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
    // Controller synced at boot — advertise only when the user has Bluetooth ON.
    if (enabled_) startAdvertisingInternal();
}

void Esp32Ble::startAdvertisingInternal() {
    struct ble_gap_adv_params adv_params{};

    // Main advertisement = flags + the PLP 128-bit service UUID. The UUID is what lets
    // Forge (Web Bluetooth) discover the device via its service filter; without it,
    // requestDevice({filters:[{services:[PLP]}]}) never matches. (Note: iOS/macOS
    // Settings still won't list a custom BLE peripheral — that's expected; use a BLE
    // scanner app, e.g. nRF Connect / LightBlue, or Chrome Web Bluetooth.)
    struct ble_hs_adv_fields fields{};
    fields.flags        = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128     = (ble_uuid128_t*)&PLP_SVC_UUID;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // Name goes in the SCAN RESPONSE so the 31-byte main packet (flags + 128-bit UUID =
    // 21 B) never overflows regardless of how long the device name is.
    struct ble_hs_adv_fields rsp{};
    rsp.name           = (uint8_t*)devName_.c_str();
    rsp.name_len       = (uint8_t)devName_.size();
    rsp.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);

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
                if (connecting_) {
                    centConnHandle_ = event->connect.conn_handle;
                    connecting_ = false;
                    if (log_) log_->info("Esp32Ble", "central connected",
                        {{"handle", std::to_string(centConnHandle_)}});
                } else {
                    connHandle_ = event->connect.conn_handle;
                    pendingNotify_.store(0, std::memory_order_relaxed);  // fresh flow-control window
                    struct ble_gap_conn_desc desc{};
                    if (ble_gap_conn_find(connHandle_, &desc) == 0) {
                        std::memcpy(peer_.addr, desc.peer_id_addr.val, 6);
                    }
                    std::snprintf(peer_.name, sizeof(peer_.name), "peer");
                    advertising_ = false;
                    if (log_) log_->info("Esp32Ble", "peer connected",
                        {{"handle", std::to_string(connHandle_)}});
                    if (poster_) poster_->post({events::BtConnected, {{"name", peer_.name}}});
                }
            } else {
                connecting_ = false;
                if (enabled_) startAdvertisingInternal();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            if (event->disconnect.conn.conn_handle == centConnHandle_) {
                centConnHandle_ = 0xFFFF;
                if (log_) log_->info("Esp32Ble", "central disconnected");
            } else {
                connHandle_ = 0xFFFF;
                peer_ = BtPeer{};
                if (poster_) poster_->post({events::BtDisconnected, {}});
                if (enabled_) startAdvertisingInternal();
            }
            break;
        case BLE_GAP_EVENT_DISC: {
            if (!scanning_ || !scanCb_) break;
            ScanResult r{};
            if (event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
                event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
                r.connectable = true;
            }
            r.rssi = event->disc.rssi;
            std::snprintf(r.mac, sizeof(r.mac),
                "%02X:%02X:%02X:%02X:%02X:%02X",
                event->disc.addr.val[5], event->disc.addr.val[4],
                event->disc.addr.val[3], event->disc.addr.val[2],
                event->disc.addr.val[1], event->disc.addr.val[0]);
            if (event->disc.length_data > 0) {
                size_t nameLen = std::min((size_t)event->disc.length_data, sizeof(r.name) - 1);
                std::memcpy(r.name, event->disc.data, nameLen);
                r.name[nameLen] = '\0';
            }
            scanCb_(scanCbUser_, r);
            break;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
            scanning_ = false;
            if (log_) log_->info("Esp32Ble", "scan complete");
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
            if (log_) log_->info("Esp32Ble", "enc change",
                {{"status", std::to_string(event->enc_change.status)}});
            if (event->enc_change.status == 0) {
                peer_.bonded = true;
                if (poster_) poster_->post({events::BtPaired, {{"name", peer_.name}}});
            }
            break;
        // ── PLP handshake diagnostics (Plan 93 Fase B) ──
        case BLE_GAP_EVENT_SUBSCRIBE:   // central enabled/disabled a CCCD (TX notify)
            if (log_) log_->info("Esp32Ble", "subscribe",
                {{"attr", std::to_string(event->subscribe.attr_handle)},
                 {"notify", std::to_string(event->subscribe.cur_notify)}});
            break;
        case BLE_GAP_EVENT_MTU:
            if (log_) log_->info("Esp32Ble", "mtu", {{"value", std::to_string(event->mtu.value)}});
            break;
        case BLE_GAP_EVENT_NOTIFY_TX:
            // Fires once per TX notification — on SUCCESS OR ERROR. Always free the slot,
            // else the counter drifts up on errors (e.g. controller buffer full) and screen
            // pacing latches off mid-session. EDONE is the indication-ack variant (we only
            // send notifications, never indications) — skip it so it can't double-decrement.
            if (event->notify_tx.status != BLE_HS_EDONE) {
                int p = pendingNotify_.load(std::memory_order_relaxed);
                if (p > 0) pendingNotify_.fetch_sub(1, std::memory_order_relaxed);
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
    // No per-chunk cap here (that splits a chunked frame mid-way → the host CRC-drops the
    // partial → screen never completes). Pacing is done at FRAME granularity in
    // BleLinkTransport::send() using txPending(). Here we just submit and track in-flight.
    struct os_mbuf* om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return false;                       // host mbuf pool exhausted → drop chunk
    int rc = ble_gatts_notify_custom(connHandle_, g_plp_tx_handle, om);
    if (rc == 0) pendingNotify_.fetch_add(1, std::memory_order_relaxed);   // count in-flight
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

// ── Central role (Plan 67) ─────────────────────────────────────────────────

bool Esp32Ble::startScan(uint32_t durationMs, ScanCallback cb, void* user) {
    if (!enabled_) return false;
    if (scanning_) stopScan();

    scanCb_ = cb;
    scanCbUser_ = user;

    ble_gap_disc_params params = {};
    params.passive = 0;          // active scan → gets device names
    params.filter_duplicates = 1;
    params.limited = 0;

    uint8_t own_addr_type = BLE_OWN_ADDR_PUBLIC;
    ble_hs_id_infer_auto(0, &own_addr_type);
    int rc = ble_gap_disc(own_addr_type, (int32_t)durationMs, &params,
                          gap_event_thunk, nullptr);
    scanning_ = (rc == 0);
    if (log_) log_->info("Esp32Ble", scanning_ ? "scanning started" : "scan start failed",
                         {{"durMs", std::to_string(durationMs)}});
    return scanning_;
}

void Esp32Ble::stopScan() {
    if (!scanning_) return;
    ble_gap_disc_cancel();
    scanning_ = false;
    scanCb_ = nullptr;
    scanCbUser_ = nullptr;
}

bool Esp32Ble::connectTo(const char* mac) {
    if (!enabled_ || !mac || !mac[0]) return false;

    ble_addr_t addr{};
    addr.type = BLE_ADDR_PUBLIC;
    // Parse "AA:BB:CC:DD:EE:FF" → uint8_t[6]
    unsigned int v[6];
    if (std::sscanf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                    &v[5], &v[4], &v[3], &v[2], &v[1], &v[0]) != 6)
        return false;
    for (int i = 0; i < 6; i++) addr.val[i] = (uint8_t)v[i];

    connecting_ = true;
    int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &addr, 5000, nullptr,
                             gap_event_thunk, nullptr);
    if (rc != 0) {
        connecting_ = false;
        if (log_) log_->warn("Esp32Ble", "connect failed", {{"mac", mac}});
        return false;
    }
    if (log_) log_->info("Esp32Ble", "connecting", {{"mac", mac}});
    return true;
}

void Esp32Ble::disconnectFrom(const char* mac) {
    (void)mac;
    if (centConnHandle_ != 0xFFFF) {
        ble_gap_terminate(centConnHandle_, BLE_ERR_REM_USER_CONN_TERM);
    }
}

} // namespace nema

#else  // ── BT disabled: no-op stubs so non-BT boards still link ──
namespace nema {
void Esp32Ble::start() {}
bool Esp32Ble::initStack() { return false; }
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
bool Esp32Ble::startScan(uint32_t, ScanCallback, void*) { return false; }
void Esp32Ble::stopScan() {}
bool Esp32Ble::connectTo(const char*) { return false; }
void Esp32Ble::disconnectFrom(const char*) {}
} // namespace nema
#endif
