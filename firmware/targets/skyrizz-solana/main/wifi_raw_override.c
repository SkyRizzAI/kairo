// Allow raw TX of deauth/disassoc (0xC0 / 0xA0) management frames.
//
// ESP-IDF's closed-source WiFi blob (libnet80211.a) gates esp_wifi_80211_tx()
// behind an internal ieee80211_raw_frame_sanity_check() that rejects deauth and
// disassoc subtypes, producing "unsupport frame type: 0c0" on the wire. This is
// what makes the WiFi Marauder "Deauth Flood" fail — it is unrelated to
// promiscuous mode.
//
// We override that check to always return ESP_OK (0). The blob's symbol is a
// GLOBAL strong symbol, so to avoid a multiple-definition error it is *weakened*
// in the prebuilt archive at build-configure time (see the skyrizz-e32 target
// CMakeLists -> tools/patch_wifi_lib.py). A weak symbol is overridden by this
// strong definition, and because esp_wifi_80211_tx() references the check by
// symbol, its call binds here instead. No instruction bytes are patched.
//
// This file MUST stay in the `main` component: ESP-IDF links main with
// --whole-archive, so this object is force-included. In any other (archive)
// component the linker would never pull it — the weak blob symbol already
// satisfies the reference — and the override would silently not take effect.
//
// Plain C so the symbol name is emitted verbatim (no C++ mangling). See
// docs/decisions/0011-binary-patch-libnet80211-deauth-tx.md.
#include <stdint.h>

int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    (void)arg;
    (void)arg2;
    (void)arg3;
    return 0;  // ESP_OK — accept every frame type, including 0xC0 / 0xA0
}
