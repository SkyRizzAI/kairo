#include "kairo/input/i_touch_driver.h"
#include "kairo/services/input_service.h"

namespace kairo {

void ITouchDriver::emitPointer(input::PointerPhase phase, uint16_t x, uint16_t y) {
    if (input_) input_->postPointer(phase, x, y);
}

} // namespace kairo
