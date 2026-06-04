#pragma once
#include "kairo/hal/camera.h"
#include "kairo/service.h"
#include <cstdint>
#include <cstddef>

namespace kairo { class Runtime; }
namespace kairo::skyrizze32 { class Xl9535; }

namespace kairo::skyrizze32 {

// Gc2145Camera — GC2145 2MP DVP camera via ESP32-S3 LCD_CAM peripheral.
// Uses esp_driver_cam (built-in IDF 5.x). SCCB init via Wire I2C (shared bus).
// Frame buffer allocated in PSRAM. captureFrame() blocks for one frame (~30 ms).
class Gc2145Camera : public kairo::ICamera, public kairo::IService {
public:
    void init(kairo::Runtime& rt, Xl9535& expander);

    // ICamera
    const char*    label()       const override { return "GC2145 2MP (FPC3)"; }
    uint16_t       frameWidth()  const override { return kFrameW; }
    uint16_t       frameHeight() const override { return kFrameH; }
    bool           isOpen()      const override { return open_; }
    bool           open()              override;
    void           close()             override;
    const uint8_t* captureFrame()      override;

    // IService
    const char* name() const override { return "Gc2145Camera"; }
    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}

    // Accessible to ISR callbacks (IRAM context cannot use friend with opaque IDF types)
    static constexpr size_t kFrameBytes = (size_t)320 * 240 * 2;
    uint8_t* frameBuf_ = nullptr;   // PSRAM DMA frame buffer
    void*    frameSem_ = nullptr;   // SemaphoreHandle_t — frame-ready signal

private:
    bool sccbInit();

    static constexpr uint16_t kFrameW = 320;
    static constexpr uint16_t kFrameH = 240;

    kairo::Runtime* rt_           = nullptr;
    Xl9535*         expander_     = nullptr;
    bool            open_         = false;
    bool            sensorInited_ = false;
    void*           camHandle_    = nullptr;  // esp_cam_ctlr_handle_t (opaque)
};

} // namespace kairo::skyrizze32
