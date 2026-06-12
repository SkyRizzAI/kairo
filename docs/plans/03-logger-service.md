# 03 — Logger Service

> Service pertama yang wajib ada (§7 master plan). Semua subsystem log lewat sini — **tidak boleh `printf()` langsung**.

- Status: ☐ Not started
- Milestone: M1 (Core Foundation)
- Depends on: 02
- Blocks: 05, 06, 09 (sink stdio), 10 (panel Logs)

---

## Goal

- Logger dengan 6 level (TRACE..FATAL), komponen/tag, dan field terstruktur opsional.
- Arsitektur **sink**: Logger mem-fan-out tiap entry ke kumpulan sink.
- Sink MVP: **Console Sink** (stdout, human-readable) & **Memory Sink** (ring buffer untuk introspeksi).
- (Sink JSON-lines untuk bridge dibuat di stage 09; di sini cukup interface sink + 2 sink dasar.)

## Scope

### In scope

- `LogEntry` struct (timestamp, level, component, message, fields).
- `ILogSink` interface + `ConsoleSink`, `MemorySink`.
- `Logger` class: API `log(level, component, msg, fields)` + helper `trace/debug/info/warn/error/fatal`.
- Format human-readable sesuai contoh master plan: `[12:00:01] [INFO ] [Runtime] Booting`.
- Integrasi ke `Runtime`: Logger di-init paling awal di `initCore()` & diakses subsystem lain.

### Out of scope

- File Sink & Remote Sink (master plan §7 "future") — ditunda.
- JSON-lines sink ke stdout untuk web (stage 09).
- Filtering level di UI (stage 10).

---

## Design

### Tipe & file

```text
firmware/core/include/palanu/log/
├─ log_entry.h     # LogEntry, Field
├─ log_sink.h      # ILogSink
└─ logger.h        # Logger
firmware/core/src/log/
├─ logger.cpp
├─ console_sink.cpp
└─ memory_sink.cpp
```

### Sketsa

```cpp
// log_entry.h
namespace nema {
struct Field { const char* key; std::string value; };
struct LogEntry {
  uint64_t   epochMs;
  LogLevel   level;
  const char* component;     // "Runtime", "WifiService", ...
  std::string message;
  std::vector<Field> fields; // structured logging (opsional)
};
}

// log_sink.h
namespace nema {
struct ILogSink {
  virtual ~ILogSink() = default;
  virtual void write(const LogEntry& e) = 0;
};
}

// logger.h
namespace nema {
class Logger {
public:
  explicit Logger(IClock& clock);
  void addSink(ILogSink& sink);
  void setMinLevel(LogLevel lvl);   // default Trace

  void log(LogLevel lvl, const char* component, std::string msg,
           std::vector<Field> fields = {});

  // helper
  void info (const char* c, std::string m, std::vector<Field> f = {}) { log(LogLevel::Info, c, std::move(m), std::move(f)); }
  void error(const char* c, std::string m, std::vector<Field> f = {}) { log(LogLevel::Error, c, std::move(m), std::move(f)); }
  // ... trace/debug/warn/fatal serupa
private:
  IClock& clock_;
  LogLevel minLevel_ = LogLevel::Trace;
  std::vector<ILogSink*> sinks_;
};
}
```

### ConsoleSink format

```text
[HH:MM:SS] [LEVEL] [Component] message  key=value key2=value2
```

- Level di-pad 5 char (`INFO `, `WARN `, `ERROR`) seperti contoh §7.
- Fields (kalau ada) di-append `k=v`.
- Tulis ke `stdout` via `std::fwrite`/`std::fputs` (ini satu-satunya tempat "boleh" sentuh stdout, dibungkus sink).

### MemorySink

- Ring buffer kapasitas tetap (mis. 1024 entry) → untuk introspeksi & nanti bisa di-dump.
- `const std::deque<LogEntry>& entries() const`.

### Integrasi Runtime

- `Runtime` punya `Logger& log()`.
- `initCore()` urutan: buat Logger (pakai `platform.clock()`), pasang `ConsoleSink` + `MemorySink`, lalu `log().info("Logger","Initialized")`.
- Mulai stage ini, transisi BootPhase di stage 02 diganti dari `fputs` mentah → `logger.info(...)`.

---

## Tasks

- [ ] `log_entry.h`, `log_sink.h`, `logger.h` + impl `logger.cpp`.
- [ ] `ConsoleSink` (format human-readable §7) + `MemorySink` (ring buffer).
- [ ] Tambah `Logger& log()` ke `Runtime`; init di `initCore()`.
- [ ] Ganti semua output mentah boot flow (stage 02) → lewat Logger.
- [ ] Tambah unit test kecil (opsional, `bun`-independent / ctest) untuk format & ring buffer.

## Acceptance criteria

- Boot mencetak log berformat persis gaya §7, contoh:
  `[12:00:02] [INFO ] [Logger] Initialized`.
- `setMinLevel(Warn)` menyaring Trace/Debug/Info.
- MemorySink menyimpan ≤ kapasitas (entry terlama ter-evict).
- Tidak ada `printf`/`std::cout` di luar `ConsoleSink`/sink (grep bersih).

## How to verify

```bash
bun run build:firmware && bun run run:firmware
# amati baris log berformat [time] [LEVEL] [Component] message
```

## Risks / notes

- `std::string`/`std::vector` di core OK untuk host & ESP32 (ada heap). Untuk band hardware sangat ketat nanti bisa dibatasi, tapi di luar scope MVP.
- Fields disimpan sebagai `value:string` agar sink JSON (stage 09) gampang; tipe kaya (int/bool) bisa ditambah belakangan.
