#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {
class Runtime;

// Settings → Secure Element. Shows the SE050 status (slots, secure-store, UID)
// and offers a "Generate random" hardware test. Gated on caps::Secure by the
// root Settings.
class SecureSettingsScreen : public ComponentScreen {
public:
    explicit SecureSettingsScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState  scroll_;
    std::vector<std::string> rows_;
    std::string              lastRandom_;   // hex of last "Generate random" test
};

} // namespace nema
