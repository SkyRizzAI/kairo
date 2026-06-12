# 12 — Plugin Runtime

> Plugin system: IPlugin interface, PluginManager (load/unload/lifecycle/permissions), PluginContext (API surface untuk plugin), dan sample plugin pertama.

- Status: ☐ Not started
- Milestone: M4 (Plugin Runtime)
- Depends on: 01–11 (MVP complete)
- Blocks: 15 (Home Screen butuh plugin), 16+ (ESP32 perlu plugin runtime)

---

## Goal

- Plugin dapat dikembangkan tanpa modifikasi Core.
- Plugin hanya boleh akses sistem lewat `PluginContext` (capability-gated).
- PluginManager mengelola lifecycle: load → init → running → unload.
- Simulator dapat load/unload plugin lewat Controls panel.
- Satu sample plugin (`HelloPlugin`) membuktikan sistem berjalan.

## Scope

### In scope

- `IPlugin` interface (id, name, version, onLoad/onUnload/onTick).
- `PluginContext`: API yang di-expose ke plugin (log, events, capabilities, registerService, registerStatusItem).
- `PluginManager`: load, unload, lifecycle, daftar plugin aktif.
- Emit events: `PluginLoaded`, `PluginUnloaded` (sudah didefinisikan di event.h).
- `HelloPlugin` sample (log "hello" tiap 5 detik).
- Integrasi ke `Runtime`: `rt.plugins()` accessor.
- Simulator Controls: tombol "Load Plugin" / "Unload Plugin" (hardcode sample untuk MVP).
- Web UI: panel **Plugins** (nama + state aktif/nonaktif).

### Out of scope

- Dynamic loading dari file `.so`/`.dll` — MVP plugin adalah static linkage.
- Plugin permission system kompleks (izin granular ditunda).
- Plugin marketplace / download.
- Plugin yang menggambar UI (perlu UI Runtime di plan 14).

---

## Design

### File

```text
firmware/core/include/palanu/plugin/
├─ plugin.h           # IPlugin, PluginId
├─ plugin_context.h   # PluginContext
└─ plugin_manager.h   # PluginManager
firmware/core/src/plugin/
├─ plugin_context.cpp
└─ plugin_manager.cpp
firmware/core/include/palanu/plugins/
└─ hello_plugin.h     # sample plugin
firmware/core/src/plugins/
└─ hello_plugin.cpp
```

### IPlugin

```cpp
namespace nema {
using PluginId = const char*;

struct IPlugin {
    virtual ~IPlugin() = default;
    virtual PluginId    id()      const = 0;  // "com.palanu.hello"
    virtual const char* name()    const = 0;  // "Hello Plugin"
    virtual const char* version() const = 0;  // "1.0.0"
    virtual void onLoad(PluginContext& ctx)   = 0;
    virtual void onUnload(PluginContext& ctx) = 0;
    virtual void onTick(PluginContext& ctx, uint64_t nowMs) {}
};
}
```

### PluginContext

Plugin hanya boleh akses sistem melalui ini — bukan langsung ke Runtime.

```cpp
namespace nema {
class PluginContext {
public:
    PluginContext(Runtime& rt, IPlugin& plugin);

    Logger&             log();
    EventBus&           events();
    CapabilityRegistry& capabilities();
    ServiceContainer&   container();

    // Register sebuah background service atas nama plugin
    void registerService(IService* svc);

    // Subscribe event — auto-unsubscribe saat plugin unload
    SubscriptionId subscribe(const char* name, EventHandler handler);
};
}
```

### PluginManager

```cpp
namespace nema {
class PluginManager {
public:
    PluginManager(Runtime& rt);

    void load(IPlugin& plugin);
    void unload(PluginId id);
    void tickAll(uint64_t nowMs);
    void unloadAll();

    bool isLoaded(PluginId id) const;
    const std::vector<IPlugin*>& plugins() const;

private:
    Runtime& rt_;
    struct Entry { IPlugin* plugin; std::unique_ptr<PluginContext> ctx; };
    std::vector<Entry> entries_;
};
}
```

### HelloPlugin (sample)

```cpp
class HelloPlugin : public IPlugin {
    PluginId    id()      const override { return "com.palanu.hello"; }
    const char* name()    const override { return "Hello Plugin"; }
    const char* version() const override { return "1.0.0"; }

    void onLoad(PluginContext& ctx) override {
        ctx.log().info("HelloPlugin", "loaded!");
        ctx.subscribe("ClockTick", [&ctx](const Event& e) {
            // log setiap 5 detik (gunakan uptime dari payload)
        });
    }
    void onUnload(PluginContext& ctx) override {
        ctx.log().info("HelloPlugin", "unloaded");
    }
};
```

### Integrasi Runtime

- `Runtime::initCore()`: buat `PluginManager`.
- `Runtime::plugins()` accessor.
- `Runtime::run()` loop: `plugins().tickAll(now)`.
- `Runtime::requestShutdown()` sebelum stopAll: `plugins().unloadAll()`.

### Integrasi Simulator

- `main.cpp`: load `HelloPlugin` setelah `rt.start()`.
- Controls panel tambah tombol "Load Hello" / "Unload Hello".
- Web UI: panel **Plugins** listing `id`, `name`, `version`, status.
- Command bridge baru: `{"cmd":"load_plugin","id":"com.palanu.hello"}` / `{"cmd":"unload_plugin","id":"..."}`.

---

## Tasks

- [ ] `plugin.h`, `plugin_context.h`, `plugin_manager.h` + impl.
- [ ] `PluginContext` auto-unsubscribe saat unload (track subscription IDs).
- [ ] `PluginManager` load/unload/tick/unloadAll + emit PluginLoaded/Unloaded.
- [ ] `HelloPlugin` sample.
- [ ] Integrasi ke Runtime (accessor + tick loop + shutdown).
- [ ] `main.cpp`: load HelloPlugin.
- [ ] Bridge: command `load_plugin`/`unload_plugin`.
- [ ] Web UI: panel Plugins + tombol di Controls.
- [ ] Core CMakeLists: tambah plugin sources.
- [ ] Verifikasi: HelloPlugin load → log "loaded" → tick → unload bersih.

## Acceptance criteria

- `HelloPlugin` load → `PluginLoaded` event ter-publish, muncul di Events panel.
- `HelloPlugin` di-unload → `PluginUnloaded` event, subscriptions bersih.
- Plugin tidak bisa akses `Runtime` langsung (hanya lewat `PluginContext`).
- Double-load plugin yang sama → log warning, tidak crash.
- `unloadAll()` dipanggil saat shutdown — semua plugin unload sebelum services stop.

## Risks / notes

- `PluginContext::subscribe` harus menyimpan `SubscriptionId` dan unsubscribe saat `onUnload` — pastikan ini terjadi otomatis di destructor PluginContext.
- Static linkage untuk MVP: plugin adalah object yang dibuat di `main.cpp` lalu di-pass ke `load()`. Dynamic loading (`.so`) adalah post-MVP.
- `onTick` dipanggil di main loop — jangan lakukan blocking I/O di dalam plugin.
