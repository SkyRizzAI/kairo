// Host link/smoke test — WalletsApp (Plan 94, Fase 6).
//
// Instantiating WalletsApp pulls its whole translation unit into the executable,
// forcing the linker to resolve every aether widget (View/ListContainer/Dialog/
// TitleBar/Text/VirtualKeyboard) and the wallet stack it builds. A clean link + the
// identity check confirms the app compiles, links, and is registrable.

#include "nema/apps/wallets_app.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include <cstdlib>
uint32_t random32(void) { return (uint32_t)arc4random(); }
void random_buffer(uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) b[i] = (uint8_t)arc4random(); }
}

int main() {
    nema::WalletsApp app;
    bool ok = std::strcmp(app.id(), "com.palanu.wallets") == 0 &&
              std::strcmp(app.name(), "Wallets") == 0 &&
              std::strcmp(app.category(), "System") == 0;  // launcher-level, hidden from Apps list
    std::printf("[%s] WalletsApp links + identity (%s / %s / %s)\n", ok ? "PASS" : "FAIL",
                app.id(), app.name(), app.category());
    return ok ? 0 : 1;
}
