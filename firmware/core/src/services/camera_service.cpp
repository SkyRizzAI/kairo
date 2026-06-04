#include "kairo/services/camera_service.h"

namespace kairo {

void CameraService::add(ICamera* drv, const char* id, const char* desc) {
    if (count_ >= kMaxDevices) return;
    entries_[count_++] = {drv, id, desc};
}

ICamera*    CameraService::get (int i) const { return (i >= 0 && i < count_) ? entries_[i].drv  : nullptr; }
const char* CameraService::id  (int i) const { return (i >= 0 && i < count_) ? entries_[i].id   : nullptr; }
const char* CameraService::desc(int i) const { return (i >= 0 && i < count_) ? entries_[i].desc : nullptr; }

} // namespace kairo
