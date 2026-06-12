#pragma once
#include "nema/hal/camera.h"

namespace nema {

class CameraService {
public:
    static constexpr int kMaxDevices = 4;

    void     add(ICamera*, const char* id, const char* desc);
    int      count()       const { return count_; }
    ICamera* get(int i)    const;
    const char* id  (int i) const;
    const char* desc(int i) const;

private:
    struct Entry { ICamera* drv; const char* id; const char* desc; };
    Entry entries_[kMaxDevices] = {};
    int   count_ = 0;
};

} // namespace nema
