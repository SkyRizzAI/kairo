#include "nema/screens/secure_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/service/service_container.h"
#include "nema/hal/secure_element.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

SecureSettingsScreen::SecureSettingsScreen(Runtime& rt) : ComponentScreen(rt, 96) {}

void SecureSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

#define S(u) static_cast<SecureSettingsScreen*>(u)

UiNode* SecureSettingsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    auto* se = rt.container().resolve<ISecureElement>();

    MenuBuilder m(a, scroll_, this);
    m.section("Secure Element");
    if (!se) {
        m.info("Not present", nullptr);
        return m.build();
    }

    char slots[16];
    std::snprintf(slots, sizeof(slots), "%u", (unsigned)se->slotCount());
    rows_.push_back(slots);
    m.info("Slots", rows_[0].c_str());
    m.info("Secure store", se->hasFeature(SeFeature::SecureStore) ? "enabled" : "software");

    std::string uid;
    if (se->uniqueId(uid) && !uid.empty()) {
        rows_.push_back(uid);
        m.info("UID", rows_.back().c_str());
    }

    m.section("Test");
    m.nav("Generate random", [](void* u){
        auto* s = S(u);
        uint8_t r[8] = {0};
        static const char* hx = "0123456789abcdef";
        std::string out;
        if (s->rt_.container().resolve<ISecureElement>() &&
            s->rt_.container().resolve<ISecureElement>()->randomBytes(r, sizeof(r))) {
            for (uint8_t byte : r) { out += hx[byte >> 4]; out += hx[byte & 0xF]; }
        } else {
            out = "(failed)";
        }
        s->lastRandom_ = out;
        s->rt_.view().requestRedraw();
    });
    if (!lastRandom_.empty())
        m.info("Random", lastRandom_.c_str());

    return m.build();
}

#undef S

} // namespace nema
