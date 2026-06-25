// Host unit test — WalletConsentService (Plan 94, Fase 5).
//
// The trusted-display consent plumbing: a worker thread blocks in requestSign while the
// "GUI" (this test) ticks, receives the request via the factory, and resolves it with a
// physical-button approve/reject. Verifies approve, reject, and fail-closed (no factory).

#include "nema/wallet/wallet_consent_service.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using namespace nema::wallet;

static int g_fails = 0;
static void ok(const char* name, bool c) { if (!c) g_fails++; std::printf("[%s] %s\n", c ? "PASS" : "FAIL", name); }

// Drive one consent round on a worker thread; the main thread acts as the GUI: tick until
// the factory captures the request, then resolve with `approve`.
static bool runRound(WalletConsentService& cs, bool approve) {
    std::shared_ptr<WalletConsentService::SignRequest> captured;
    cs.setScreenFactory([&](auto req) { captured = req; });

    std::atomic<bool> result{false}, finished{false};
    TxPreview preview;
    preview.rows.push_back({"To", "0xabc"});
    std::thread worker([&] {
        result = cs.requestSign(preview, "app.example.org", BackendKind::SecureElement);
        finished = true;
    });

    while (!captured) { cs.guiTick(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    // The captured request carries exactly what the screen would render.
    ok("request carries preview + origin + backend",
       captured->origin == "app.example.org" && captured->backend == BackendKind::SecureElement &&
       captured->preview.rows.size() == 1);
    captured->resolve(approve);

    worker.join();
    return result;
}

int main() {
    // Fail-closed: no screen factory wired → can't obtain consent → reject.
    {
        WalletConsentService cs;
        TxPreview p;
        ok("no factory → fail-closed reject", !cs.requestSign(p, "x", BackendKind::Software));
    }

    {
        WalletConsentService cs;
        ok("physical approve → true", runRound(cs, true) == true);
    }
    {
        WalletConsentService cs;
        ok("physical reject → false", runRound(cs, false) == false);
    }

    std::printf("\n%s (%d failure%s)\n", g_fails ? "WALLET CONSENT TESTS FAILED" : "ALL WALLET CONSENT TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
