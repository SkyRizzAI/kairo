#include "nema/skyrizze32/se050_driver.h"
#include "nema/skyrizze32/board_config.h"
#include "nema/skyrizze32/xl9535.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include <Wire.h>
#include <Arduino.h>

namespace nema::skyrizze32 {

void Se050Driver::init(Runtime& rt, Xl9535& expander) {
    rt_       = &rt;
    expander_ = &expander;

    // Reset pulse via XL9535 P03. asserted=true drives the reset line active;
    // hold briefly, release, then let the chip boot before probing.
    expander.setSeReset(true);
    delay(2);
    expander.setSeReset(false);
    delay(10);

    // I²C presence probe on the shared bus (Wire already begun by the board).
    Wire.beginTransmission(I2C_ADDR_SE050);
    present_ = (Wire.endTransmission() == 0);

    if (present_)
        rt.log().info("SE050", "present (scaffold — crypto ops are TODO)",
                      {{"addr", "0x48"}, {"bus", "shared I2C"}});
    else
        rt.log().warn("SE050", "no ACK on I2C — secure element absent or held in reset",
                      {{"addr", "0x48"}});
}

// ── Operations: TODO (ADR 0005) ──────────────────────────────────────────────
// These require the NXP Plug-and-Trust middleware to open an APDU session over
// I²C and physical hardware to validate against. Until then they fail closed so
// callers fall back to software crypto rather than trusting a fake result.

bool Se050Driver::randomBytes(uint8_t* /*out*/, size_t /*n*/) {
    return false;   // TODO: Se05x_API_GetRandom
}

bool Se050Driver::uniqueId(std::string& /*out*/) const {
    return false;   // TODO: read factory-provisioned unique ID object
}

bool Se050Driver::genKey(uint8_t /*slot*/, SeKeyType /*type*/) {
    return false;   // TODO: Se05x_API_WriteECKey (generate-in-place)
}

bool Se050Driver::publicKey(uint8_t /*slot*/, std::vector<uint8_t>& /*out*/) const {
    return false;   // TODO: Se05x_API_ReadObject (public part only)
}

bool Se050Driver::sign(uint8_t /*slot*/, const uint8_t* /*digest*/, size_t /*digestLen*/,
                       std::vector<uint8_t>& /*sig*/) {
    return false;   // TODO: Se05x_API_ECDSASign
}

bool Se050Driver::verify(uint8_t /*slot*/, const uint8_t* /*digest*/, size_t /*digestLen*/,
                         const uint8_t* /*sig*/, size_t /*sigLen*/) const {
    return false;   // TODO: Se05x_API_ECDSAVerify
}

} // namespace nema::skyrizze32
