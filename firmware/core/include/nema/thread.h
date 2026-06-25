#pragma once
#include <atomic>
#include <cstdint>

// Nema kernel — portable thread primitive.
// ESP32: FreeRTOS task (full control over stack/core/priority).
// Host:  std::thread.
// Header stays platform-clean: handles kept as void*, real types live in
// thread_esp32.cpp / thread_host.cpp.
namespace nema {

struct ThreadConfig {
    const char* name       = "nema_thread";
    uint32_t    stackBytes = 4096;   // ESP32: task stack (bytes, ESP-IDF convention)
    uint8_t     priority   = 5;      // ESP32 FreeRTOS prio. Host: ignored.
    int8_t      core       = -1;     // ESP32: 0/1, -1 = no affinity. Host: ignored.
    bool        psram      = false;  // ESP32: force stack in PSRAM (WithCaps). Use for
                                     // late-spawned threads when internal RAM is scarce
                                     // — but NOT for threads that run during cache-disable
                                     // (flash writes) or hard ISR context.
};

class Thread {
public:
    using Entry = void (*)(void* arg);

    Thread() = default;
    ~Thread();                                   // requestStop() + join()
    Thread(const Thread&)            = delete;
    Thread& operator=(const Thread&) = delete;

    // Start running entry(arg) on a new thread. Call once.
    // The entry body MUST loop `while (!owner.shouldStop())` and return on stop —
    // there is no safe way to forcibly kill a thread holding a lock.
    void start(const ThreadConfig& cfg, Entry entry, void* arg);

    void requestStop();                          // cooperative stop signal
    bool shouldStop() const { return stop_.load(std::memory_order_relaxed); }
    bool running()    const { return running_.load(std::memory_order_relaxed); }
    void join();                                 // block until entry returns

private:
    static void trampoline(void* self);          // ESP32 task entry

public:
    // Sleep the calling thread. FreeRTOS vTaskDelay / std::this_thread::sleep_for.
    static void sleepMs(uint32_t ms);

private:
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
    Entry             entry_ = nullptr;
    void*             arg_   = nullptr;
    void*             os_    = nullptr;   // host: std::thread* | esp32: TaskHandle_t
    [[maybe_unused]] void* done_ = nullptr;  // esp32 only: completion SemaphoreHandle_t
    // esp32 only: stack allocated in PSRAM via xTaskCreate…WithCaps. Such a task
    // must be deleted by another context (vTaskDeleteWithCaps), not self-deleted,
    // so join() does the delete after the task parks itself.
    [[maybe_unused]] bool capsTask_ = false;
};

} // namespace nema
