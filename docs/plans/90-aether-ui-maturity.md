# Plan 90 — Aether UI Maturity

> Mematangkan Aether UI system dari "cukup jalan" menjadi **production-ready open-source UI framework** yang bisa dijadikan referensi developer komunitas. Target: setara ergonomics dan kelengkapan React Native / Ink, dalam constraint 1-bit monochrome embedded.

**Depends on:** Plan 79 (Layer 3 ListView), Plan 70 (Modal/Dialog), Plan 53 (Icons)
**Status:** Planning

---

## 1. Masalah yang Diselesaikan

Hasil audit kode (Juni 2026) menemukan 12 area yang perlu diperbaiki sebelum framework ini layak dijadikan public API. Diurutkan dari paling kritis:

| ID | Masalah | Risiko |
|---|---|---|
| U01 | Silent arena overflow (nullptr diam-diam) | Runtime crash |
| U02 | `const char* text` lifetime tidak di-enforce | Dangling pointer |
| U03 | `void*` callbacks — tidak ergonomis di C++17 | DX buruk |
| U04 | Style: hanya uniform padding, tidak ada margin/min/max | Layout terbatas |
| U05 | Text node tidak support multiline/word-wrap | Feature gap |
| U06 | Tidak ada `position: absolute` | Tidak bisa overlay/badge |
| U07 | List tidak ada virtualization (semua item di-layout) | Performa jeblok |
| U08 | Tidak ada screen transition | Terasa cheap |
| U09 | Tidak ada shared state (Context/Provider pattern) | Architecture gap |
| U10 | Namespace split `nema::ui` vs `aether::ui` | Confusing contributor |
| U11 | Tidak ada dev tools / tree inspector | Debugging susah |
| U12 | Focus max hardcoded 64 | Silent breakage |

---

## 2. Keputusan Arsitektur: Standar vs Dinamis

### 2.1 Pertanyaan

> *"Rule limits seperti max node dan arena size — haruskah dibuat dinamis per board? Kalau iya, apakah apps bisa berbeda behavior di board berbeda? Haruskah ada standar yang dipaksakan?"*

### 2.2 Jawaban: Universal Floor + Board Profile

Model yang dipakai adalah **dua lapisan**, konsisten dengan capability system yang sudah ada:

```
┌─────────────────────────────────────────────────────────┐
│  Universal Floor  (semua board HARUS support)           │
│  • 128 nodes minimum                                    │
│  • 48 focusable minimum                                 │
│  • Scroll (tanpa momentum) — minimum behavior           │
│  Apps yang stay dalam floor ini: jalan di mana saja     │
├─────────────────────────────────────────────────────────┤
│  Board-Declared Profile  (opsional, di atas floor)      │
│  • caps::UiExtended   → 512+ nodes, 128 focusable       │
│  • caps::UiMomentum   → flick scroll + momentum         │
│  • caps::UiTransitions→ screen push/pop animation       │
│  • caps::UiAnimations → tween/spring value animation    │
└─────────────────────────────────────────────────────────┘
```

Board deklarasi profile di `describeHardware()`:
```cpp
// firmware/boards/skyrizz-e32/skyrizz_e32_board.cpp
void SkyRizzE32Board::describeHardware(Runtime& rt) {
    // ... existing caps ...
    rt.capabilities().add(caps::UiExtended);    // 4MB PSRAM → bisa 512 nodes
    rt.capabilities().add(caps::UiMomentum);    // 240MHz → momentum OK
}
```

App deklarasi minimum tier di manifest/interface:
```cpp
// App yang butuh lebih dari floor
const char* const* requiredCaps() const override {
    static const char* reqs[] = { caps::UiExtended, nullptr };
    return reqs;
}
```

Launcher yang sudah ada sudah filter by capabilities — tidak perlu logic baru.

### 2.3 Mengapa 128 Nodes Cukup sebagai Floor

Dengan VirtualList (fase 3 plan ini), node count bukan bottleneck untuk list panjang. VirtualList hanya allocate `visible_items + 2` nodes terlepas dari total count. Screen dengan 500-item list tetap butuh 15-20 nodes.

Perkiraan screen nyata:
- Settings screen (50 opsi): ~80 nodes (header + ListContainer + rows visible)
- Home screen carousel: ~30 nodes
- Dialog + buttons: ~15 nodes

128 nodes mencukupi semua screen standard tanpa mode extended.

### 2.4 Kontrak Portabilitas Apps

App **HARUS** tetap correct (tidak crash, tidak display garbage) di minimum tier. App **BOLEH** lebih kaya fitur di tier yang lebih tinggi.

```cpp
// Pola yang direkomendasikan untuk app yang adaptive
UiNode* build(NodeArena& a, AppContext& ctx) override {
    if (ctx.rt().capabilities().has(caps::UiExtended)) {
        return buildRichLayout(a, ctx);   // full layout
    }
    return buildCompactLayout(a, ctx);    // stripped-down tapi benar
}
```

Ini identik dengan pola `Platform.select()` di React Native dan `@supports` di CSS.

---

## 3. UiProfile Struct (New)

Satu sumber kebenaran untuk resource limits, dideklarasi per board:

```cpp
// firmware/core/include/nema/ui/ui_profile.h  (NEW)
namespace aether::ui {

struct UiProfile {
    // Arena & focus
    uint16_t maxNodes      = 128;   // default universal floor
    uint16_t maxFocusable  = 48;

    // Behavioral flags — false = feature ada tapi dimatikan
    bool momentumScroll    = false;  // flick + velocity decay
    bool screenTransitions = false;  // slide/fade push/pop
    bool valueAnimations   = false;  // tween/spring Animated API

    // Derived from capabilities by Runtime — apps read, boards write
    static UiProfile fromCapabilities(const CapabilityRegistry& caps);
};

} // namespace aether::ui
```

Runtime expose: `rt.ui().profile()`. Board tidak perlu set UiProfile manual — `fromCapabilities()` build dari capability flags, single source of truth.

---

## 4. Fase Implementasi

### Fase 1 — Fondasi & Safety (Prioritas: Critical)
*Target: tidak ada silent failures, namespace bersih, debug tools.*

**F1.1 — Arena safety + overflow protection** [U01]

`NodeArena::alloc()` saat ini return `nullptr` diam-diam. Fix:

```cpp
UiNode* NodeArena::alloc() {
    if (used_ >= cap_) {
        // Debug build: assert
        // Release build: log warning (satu kali per frame)
        if (!overflowLogged_) {
            log().warn("NodeArena", "overflow — tree truncated",
                       {{"cap", std::to_string(cap_)},
                        {"screen", currentScreenName_}});
            overflowLogged_ = true;
        }
        return &sentinel_;  // sentinel node renders nothing, prevents null deref
    }
    // ...
}
```

Tambah `size_t overflowCount()` untuk dev tools (F1.3).

**F1.2 — String arena untuk lifetime safety** [U02]

Masalah: `const char* text` tidak dimiliki node, caller bisa pass pointer ke string lokal yang hilang setelah `build()` return.

```cpp
// firmware/core/include/nema/ui/string_arena.h  (NEW)
class StringArena {
public:
    explicit StringArena(size_t capacity);  // e.g. 512 bytes
    const char* intern(const char* str);    // copy into arena, return stable ptr
    const char* intern(const std::string& s) { return intern(s.c_str()); }
    void reset();  // O(1), called alongside NodeArena::reset()
    // ...
};
```

`ComponentApp` dan `ComponentScreen` tambah `StringArena strArena_` member. Widget builders yang accept `const char*` punya dua overloads: satu terima pointer raw (caller guarantees lifetime, untuk string literal), satu terima `StringArena&` untuk dynamic strings.

Dokumentasi jelas di header: literal string → pass langsung, dynamic string → `sa.intern(str)`.

**F1.3 — Tree inspector / debug dump** [U11]

```cpp
// aether_debug.h  (NEW)
namespace aether::ui::debug {
    // Dump tree structure + computed layout ke rt.log()
    void dumpTree(const UiNode* root, nema::ILogger& log, int depth = 0);
    // Single-line summary: node count, overflow count, focusable count
    void dumpStats(const NodeArena& a, const ComponentState& st, nema::ILogger& log);
}
```

CLI command `ui dump` (register via `rt.cli()`) yang trigger dump pada active screen. Invaluable untuk contributor debugging layout issues.

**F1.4 — Unifikasi namespace** [U10]

Hapus bridge `namespace nema { namespace ui = ::aether::ui; }`. Ganti semua occurrences `nema::ui` → `aether::ui` di seluruh codebase. Satu namespace, satu nama. Proses: grep + sed + compile check.

**F1.5 — Dynamic focus limit dari UiProfile** [U12]

```cpp
// focus.h
void focusedNode(UiNode& root, FocusState& fs, uint16_t maxFocusable);
// ComponentRuntime passes rt.ui().profile().maxFocusable
```

Hapus hardcoded `64`.

---

### Fase 2 — Layout Completeness (Prioritas: High)
*Target: semua layout pattern umum bisa diekspresikan.*

**F2.1 — Per-side padding + margin** [U04]

```cpp
struct Style {
    // Ganti: uint8_t padding = 0;
    // Dengan:
    uint8_t pt = 0, pr = 0, pb = 0, pl = 0;  // padding top/right/bottom/left
    int8_t  mt = 0, mr = 0, mb = 0, ml = 0;  // margin (bisa negatif untuk overlap)

    // Convenience setters (backward compat):
    Style& padding(uint8_t all)       { pt=pr=pb=pl=all; return *this; }
    Style& paddingH(uint8_t h)        { pl=pr=h; return *this; }
    Style& paddingV(uint8_t v)        { pt=pb=v; return *this; }
    Style& margin(int8_t all)         { mt=mr=mb=ml=all; return *this; }
};
```

Existing code yang pakai `style.padding = N` masih compile (field dihapus, ganti ke setter, atau sementara keep via union trick).

**F2.2 — minWidth / maxWidth / minHeight / maxHeight** [U04]

```cpp
struct Style {
    uint16_t minW = 0;
    uint16_t maxW = SIZE_AUTO;  // SIZE_AUTO = unconstrained
    uint16_t minH = 0;
    uint16_t maxH = SIZE_AUTO;
};
```

Layout engine pass 2 (size distribution) clamp ke min/max setelah flexGrow distribution.

**F2.3 — Multiline Text node** [U05]

```cpp
// node.h
struct UiNode {
    // existing text fields +
    bool    wrap       = false;  // word-wrap to node width
    uint8_t lineGap    = 1;      // px between lines (default 1)
    // maxLines = 0 means unlimited; > 0 = clamp + "..." on last line
    uint8_t maxLines   = 0;
};
```

Layout: saat `wrap=true` dan `width` fixed, ukur actual height dari `draw::measureMultiline(text, width, role)`. Renderer: pakai `draw::multiline()` yang sudah ada.

Builder convenience: `MultilineText(arena, text, role, maxLines=0)`.

**F2.4 — Position Absolute** [U06]

```cpp
enum class Position : uint8_t { Relative, Absolute };

struct Style {
    Position position = Position::Relative;
    int16_t  absX = 0;   // relative to parent's top-left (if Absolute)
    int16_t  absY = 0;
};
```

Layout: absolute nodes dikeluarkan dari flow (tidak berkontribusi ke parent's content size). Renderer: paint absolute children SETELAH semua relative children selesai (painter's algorithm — no z-index needed, absolute always on top).

Use cases: badge di atas icon, floating hint text, overlay indicator.

---

### Fase 3 — Performance & Interaksi (Prioritas: High)
*Target: performa tidak jeblok di list panjang, key prop untuk stabilitas.*

**F3.1 — VirtualList** [U07]

Node khusus yang hanya render visible items. Ini yang membuat universal floor 128 nodes cukup untuk list 1000 item.

```cpp
// widgets.h
struct VirtualListItem {
    void (*renderItem)(NodeArena& a, int index, void* userdata);
    void*       userdata;
    bool        focusable;  // apakah item ini bisa difocus
};

UiNode* VirtualList(NodeArena& a, ScrollState& st,
                    int totalCount,
                    uint16_t estimatedItemHeight,
                    VirtualListItem* items,   // array, count = totalCount
                    Style style = {});
```

Layout: VirtualList claim full viewport height. Berdasarkan `st.scrollMain` dan `estimatedItemHeight`, hitung `firstVisible` dan `lastVisible` index. Hanya allocate dan layout `lastVisible - firstVisible + 2` nodes (buffer 1 di atas dan bawah untuk smooth scroll).

Fokus: ComponentRuntime track `focusedIndex` (persistent, int) bukan pointer ke node (yang stale setelah rebuild). Auto-scroll focused item into view via ScrollState adjustment.

**F3.2 — Key prop untuk stable scroll state** [U09-adjacent]

Masalah: kalau list items berubah urutan/konten, `ScrollState` dan `FocusState` out of sync.

```cpp
// node.h
struct UiNode {
    const char* key = nullptr;  // optional stable identifier for reconciliation
};
```

ComponentRuntime: saat Prev/Next navigation, resolve focus by key (tidak by index). Kalau focused key hilang dari tree, fall back ke index clamp.

**F3.3 — Callback ergonomics: std::function support** [U03]

Tambah overloads yang accept `std::function<void()>` atau lambda via FunctionArena:

```cpp
// function_arena.h  (NEW)
// Inline storage (15 bytes) — zero heap allocation untuk common lambdas
template<typename F, size_t InlineBytes = 15>
class InplaceFn { /* ... */ };

// function_arena: pool dari InplaceFn untuk callbacks per frame
class FunctionArena {
public:
    // Store a callable, return stable void(*)(void*) + void* pair
    template<typename F>
    std::pair<void(*)(void*), void*> store(F&& fn);
    void reset();
};
```

Widget builders tambah overload:
```cpp
// Selain existing:
UiNode* Button(NodeArena& a, FunctionArena& fa, const char* label, auto&& onPress);
UiNode* Pressable(NodeArena& a, FunctionArena& fa, auto&& onPress, Style s, ...);
```

FunctionArena direset bersamaan dengan NodeArena. Total overhead: `sizeof(InplaceFn) * count` — fixed pool, zero heap.

`ComponentApp` dan `ComponentScreen` expose `FunctionArena& fa()` atau terima sebagai parameter ke `build(NodeArena&, FunctionArena&, AppContext&)`.

**F3.4 — LazyLoader + Skeleton UI** [U-new]

Untuk directory listing besar (SD card, file browser, app list dari network). Mekanisme: background reading + VirtualList dengan placeholder row untuk items yang belum dimuat.

**Masalah yang diselesaikan:** `IFileSystem::listDir()` (dan FAT/LittleFS di bawahnya) membaca entries sequential dari media lambat (SD via SPI ~2-5ms/entry). Blocking read 200 entries = 400ms UI freeze. Flipper mengatasi ini dengan lazy reading + loading indicator — Palanu bisa lebih elegan karena apps punya dedicated thread.

**`LazyDirLoader` helper class:**

```cpp
// firmware/core/include/nema/ui/lazy_dir_loader.h  (NEW)
namespace aether::ui {

enum class LoadState : uint8_t { Idle, Loading, Done, Error };

// Reads directory entries from IFileSystem in background batches.
// Thread-safe: call from app thread; loader uses AsyncPoster to notify.
class LazyDirLoader {
public:
    struct Entry {
        char     name[64];
        bool     isDir;
        uint32_t size;
        bool     loaded = false;  // false = placeholder slot
    };

    explicit LazyDirLoader(nema::IFileSystem& fs);

    // Start (or restart) loading from path. Clears existing entries.
    void open(const char* path);

    // Cancel in-progress load (e.g. user navigated away).
    void cancel();

    // Called from VirtualList's renderItem to get entry at index.
    // If index >= loadedCount(), returns placeholder entry (loaded=false).
    const Entry* entryAt(int index) const;

    // Total known entry count (grows as loading progresses).
    int  count()       const;
    int  loadedCount() const;
    LoadState state()  const;

    // Poll from app's onTick() — returns true if new entries arrived
    // since last call (signals build() should rerun).
    bool poll();

    // Hint: VirtualList calls this when approaching end of loaded range.
    // Triggers next batch read if not already in progress.
    void prefetchUpTo(int index);

private:
    static constexpr int BATCH_SIZE = 32;  // entries per read cycle
    // ...
};

} // namespace aether::ui
```

**Integrasi dengan VirtualList:**

```cpp
// Di FileBrowserApp::build():
UiNode* build(NodeArena& a, AppContext& ctx) override {
    int total = loader_.count();
    return VirtualList(a, scroll_, total, 10 /*item height*/,
        [](NodeArena& a, int i, void* ud) -> UiNode* {
            auto* self = static_cast<FileBrowserApp*>(ud);
            const auto* e = self->loader_.entryAt(i);

            // Item sudah dimuat → tampilkan normal
            if (e && e->loaded) {
                return ListItemRow(a, {.label = e->name,
                                       .leftIcon = e->isDir ? k_folder : k_file,
                                       .iconW = 8, .iconH = 8});
            }
            // Belum dimuat → skeleton placeholder
            return SkeletonRow(a);
        }, this);
}

bool onTick(AppContext& ctx) override {
    if (loader_.poll()) {
        return true;  // new entries → rebuild
    }
    return false;
}
uint32_t tickIntervalMs() const override { return 100; }
```

**`SkeletonRow` widget (new):**

```cpp
// widgets.h
// Animated placeholder row — dashed rect that pulses every N ticks.
// Uses AnimatedValue (F4.2) if caps::UiAnimations available,
// otherwise simple odd/even tick toggle (compatible with all boards).
UiNode* SkeletonRow(NodeArena& a, uint16_t width = SIZE_AUTO);

// Generic skeleton block for non-list contexts (image placeholder, etc.)
UiNode* SkeletonBlock(NodeArena& a, uint16_t w, uint16_t h);
```

Di 1-bit monochrome, skeleton animation = alternating dashed/solid rect tiap 3-4 ticks (~150ms interval). Tidak butuh `caps::UiAnimations` — cukup `tick()` counter di ComponentState.

**`prefetchUpTo()` flow:**

VirtualList dalam `renderItem()` call `loader_.prefetchUpTo(i + BATCH_SIZE)` untuk items mendekati akhir range yang dimuat. LazyDirLoader queue batch read berikutnya via `AsyncPoster` (yang sudah ada di platform). Saat batch selesai, `poll()` return true → app rebuild → VirtualList tampilkan items baru.

**State visual:**

| Kondisi | Yang Ditampilkan |
|---|---|
| Loading pertama | Beberapa SkeletonRow (count estimasi dari dir entry count jika tersedia) |
| Loading progresif | Real items di atas, SkeletonRow di bawah |
| Error (kartu SD dicabut) | Toast/error row dengan pesan |
| Done | Semua real items, no skeleton |

---

### Fase 4 — Animation & Transition (Prioritas: Medium)
*Target: UI terasa polished dan responsive. Gated di `caps::UiMomentum` dan `caps::UiTransitions`.*

**F4.1 — Screen transitions** [U08]

```cpp
enum class Transition : uint8_t {
    None,      // default, instant cut
    SlideLeft, // new screen slides in from right
    SlideRight,// new screen slides in from left (back gesture)
    Fade,      // dither-based fade (1-bit safe)
};

// ViewDispatcher
void navigate(IScreen& screen, Transition t = Transition::None);
void goBack  (Transition t = Transition::SlideRight);
```

AetherServer: detect pending transition → render source + target → interpolate X offset over 8 ticks (~160ms at 20fps) → settle. Hanya aktif kalau `rt.ui().profile().screenTransitions == true`.

**F4.2 — Animated values (Animated API)** [U-new]

Untuk boards dengan `caps::UiAnimations`:

```cpp
// animated_value.h  (NEW)
struct AnimatedValue {
    float value;
    float target;
    float velocity;   // spring physics

    void animateTo(float target, float stiffness = 200.f, float damping = 20.f);
    void snapTo(float v) { value = target = v; velocity = 0; }
    bool isSettled() const;
    bool tick(float dtMs);  // returns true while still animating
};
```

UiNode dapat menyimpan pointer ke AnimatedValue untuk `x`, `y`, `opacity` (simulasi opacity di 1-bit via dither), `scale`. ComponentRuntime tick semua live AnimatedValues dan request redraw selama ada yang belum settled.

---

### Fase 5 — Shared State & DX (Prioritas: Medium)
*Target: patterns yang familiar untuk React developer.*

**F5.1 — Context API (shared state)** [U09]

Runtime sudah jadi service container. Extend dengan typed context slots:

```cpp
// runtime.h — tambahkan
template<typename T>
void  setContext(T&& value);  // store by type, moves into slot
template<typename T>
T*    context();              // returns nullptr if not set
```

Screen/app consume: `rt.context<AppTheme>()`. Tidak ada tree traversal — global per-runtime. Cukup untuk use case: theme, user session, config flags.

**F5.2 — Ref system (imperative control)** [U-new]

```cpp
// node.h
struct NodeRef {
    UiNode* node = nullptr;  // filled by layout, valid until next arena reset
    bool    valid() const { return node != nullptr; }
    // Convenience — safe even if node == nullptr:
    int16_t x() const { return node ? node->x : 0; }
    int16_t y() const { return node ? node->y : 0; }
    uint16_t w() const { return node ? node->w : 0; }
    uint16_t h() const { return node ? node->h : 0; }
};
```

```cpp
// widgets.h
// Attach a ref to a node — layout fills it
UiNode* withRef(UiNode* node, NodeRef& ref);
```

ComponentRuntime: setelah layout, traverse tree dan fill all attached refs.

Use cases: `scrollIntoView(ref)` — adjust ScrollState supaya ref node masuk viewport; `focusNode(ref)` — set FocusState ke index of ref node.

---

## 5. Interface Changes & Migration

### Breaking changes

| Change | Migration |
|---|---|
| `style.padding = N` → `style.padding(N)` | Sed: `\.padding\s*=\s*(\d+)` → `.padding($1)` |
| `nema::ui::` → `aether::ui::` | Sed + compile check |
| `build(NodeArena&, AppContext&)` → `build(NodeArena&, StringArena&, AppContext&)` | Default impl di base class untuk backward compat, override untuk new DX |

### Non-breaking additions

Semua fase lain (VirtualList, AnimatedValue, Context, Ref, Transition) adalah additive — existing code tidak perlu berubah.

---

## 6. UiProfile Integration (Board Declaration)

```cpp
// firmware/core/include/nema/ui/ui_profile.h  (NEW — Fase 1.5)
namespace aether::ui {

struct UiProfile {
    uint16_t maxNodes      = 128;   // universal floor
    uint16_t maxFocusable  = 48;
    bool     momentum      = false;
    bool     transitions   = false;
    bool     animations    = false;

    static UiProfile fromCapabilities(const nema::CapabilityRegistry& caps) {
        UiProfile p;
        if (caps.has(caps::UiExtended))    { p.maxNodes = 512; p.maxFocusable = 128; }
        if (caps.has(caps::UiMomentum))      p.momentum    = true;
        if (caps.has(caps::UiTransitions))   p.transitions  = true;
        if (caps.has(caps::UiAnimations))    p.animations   = true;
        return p;
    }
};

} // namespace aether::ui
```

Capability constants baru di `caps.h`:
```cpp
namespace nema::caps {
    constexpr auto UiExtended    = "ui.extended";    // 512+ nodes
    constexpr auto UiMomentum    = "ui.momentum";    // flick scroll
    constexpr auto UiTransitions = "ui.transitions"; // screen anim
    constexpr auto UiAnimations  = "ui.animations";  // value anim
}
```

Runtime init: `uiProfile_ = UiProfile::fromCapabilities(capabilities_)`.

Board declare:
```cpp
// skyrizz_e32_board.cpp
rt.capabilities().add(caps::UiExtended);
rt.capabilities().add(caps::UiMomentum);
// transitions: tidak, karena display SPI agak lambat
```

```cpp
// wasm_platform.cpp  
rt.capabilities().add(caps::UiExtended);    // browser = no RAM constraint
rt.capabilities().add(caps::UiMomentum);
rt.capabilities().add(caps::UiTransitions); // smooth di browser
rt.capabilities().add(caps::UiAnimations);
```

---

## 7. Portabilitas App — Kontrak Formal

**Rule 1:** App yang tidak declare `requiredCaps` HARUS correct di universal floor (128 nodes, no momentum).

**Rule 2:** App yang butuh lebih dari floor HARUS declare via `requiredCaps()`. Launcher tidak tampilkan app ke user kalau device tidak support.

**Rule 3:** App yang adaptive (render berbeda per tier) HARUS tidak crash di tier manapun. Boleh fitur-lebih di tier tinggi.

**Rule 4:** Apps DILARANG hardcode node count limit. Gunakan VirtualList untuk semua scrollable list > 10 items.

---

## 8. Tasks

### Fase 1 — Fondasi & Safety
- [x] F1.1 — Arena sentinel node + overflow warning
- [x] F1.2 — StringArena implementation + integration ke ComponentApp/Screen
- [x] F1.3 — `aether::ui::debug::dumpTree()` + `dumpStats()`
- [x] F1.4 — Namespace unifikasi (`nema::ui` → `aether::ui`)
- [x] F1.5 — `UiProfile` struct + `caps::Ui*` constants + Runtime integration
- [x] F1.6 — Dynamic focus limit (tree-walk, no MAX_FOCUS cap)

### Fase 2 — Layout Completeness
- [x] F2.1 — Margin (mt/mr/mb/ml) di Style + layout engine
- [x] F2.2 — minW/maxW/minH/maxH di Style + layout engine
- [x] F2.3 — Multiline Text node (`wrap=true`, `maxLines`, `lineGap`)
- [x] F2.4 — `position: absolute` di Style + layout + renderer

### Fase 3 — Performance & Interaksi
- [x] F3.1 — VirtualList node + VirtualListState index-based focus
- [x] F3.2 — Key prop di UiNode (field `const char* key` added)
- [x] F3.3 — FunctionArena + lambda overloads untuk widget builders
- [x] F3.4 — LazyDirLoader + SkeletonRow/SkeletonBlock widgets + progressive reveal

### Fase 4 — Animation & Transition
- [ ] F4.1 — Screen transitions (SlideLeft/Right/Fade) + ViewDispatcher API
- [ ] F4.2 — AnimatedValue (spring physics) + UiNode integration

### Fase 5 — Shared State & DX
- [x] F5.1 — Context API di Runtime (`setContext<T>`, `context<T>`)
- [x] F5.2 — NodeRef system + `withRef()` builder + `scrollIntoView(ref)`

### Fase 6 — Screen & App Modernization

Rewrite / redesign semua built-in screens dan custom apps menggunakan widget
pipeline baru (Plan 79 ListView, VirtualList, LazyDirLoader, SkeletonRow) plus
design tokens yang konsisten. Tujuan: semua layar jadi contoh nyata public API
yang bisa dijadikan referensi developer komunitas.

**Priority tiers:**

| Tier | Screens | Alasan |
|---|---|---|
| P0 — most-visited | LockScreen, DesktopScreen, LauncherScreen, AppListScreen, FileBrowserScreen, LogsScreen, SettingsScreen | User lihat setiap sesi |
| P1 — frequent | BadUsbApp, WifiSettingsScreen, AppDetailScreen, StorageSettingsScreen, AppearancesSettingsScreen | Sering diakses |
| P2 — occasional | AboutScreen, ControlsScreen, DeveloperScreen, DolphinDemoScreen, SleepSettingsScreen, SoundsSettingsScreen | Kadang diakses |
| P3 — sub-screens | WifiNetworkDetailScreen, WifiIpConfigScreen, BluetoothSettingsScreen, TouchSettingsScreen, CameraSettingsScreen, ProfileSettingsScreen, RemoteSettingsScreen, DesktopSettingScreen, FileTextViewerScreen | Sub-menu / detail |
| P4 — apps | HelloApp (demo), DolphinApp, JsApp, WasmApp | Refactor minimal |

#### P0 — Most-visited

- [x] **F6.01 — LockScreen** — Uses `hintFor(Activate)` for unlock hint (already correct).
- [x] **F6.02 — DesktopScreen** — Wallpaper uses contentY() to draw below status bar (already correct).
- [x] **F6.03 — LauncherScreen** — All 4 themes audited: no hardcoded nav hints; icon sizes use theme metrics from style_tokens. hintFor() N/A (launcher has no text footer).
- [x] **F6.04 — AppListScreen** — Ganti scroll+build manual dengan **VirtualList** + alphabetical sort. VirtualList + renderAppItem + selfHighlight XOR focus indicator.
- [ ] **F6.05 — FileBrowserScreen** — Ganti listing ke LazyDirLoader + VirtualList. Sorting sudah ada di LazyDirLoader (dirs first, alpha). Context menu (FileOpsModal) dipertahankan.
- [x] **F6.06 — LogsScreen** — ScrollView correct (non-selectable list); auto-scroll-to-bottom via 0x7FFF; TextRole::Mono per entry; level badge [T/D/I/W/E/F] already prepended.
- [x] **F6.07 — SettingsScreen** — ListContainer + ListItemRow, capability-gated skip, already correct.

#### P1 — Frequent

- [ ] **F6.08 — BadUsbApp** — State machine (Main → ScriptList → Running) sudah benar. ScriptList: LazyDirLoader + VirtualList untuk listing .dd files. Running state: tampilkan progress bar menggunakan Slider (read-only, value driven oleh byte counter).
- [x] **F6.09 — WifiSettingsScreen** — Toggle + ListItemRow for networks (functional). SkeletonRow for scanning is enhancement; RSSI icon is cosmetic — both deferred.
- [x] **F6.10 — AppDetailScreen** — Toggle widget for permissions already in use; Reset All Permissions added (Plan 90 session).
- [ ] **F6.11 — StorageSettingsScreen** — Progress bar untuk kapasitas (Slider read-only). SD card eject button melalui ListItemRow (chevron=false, accessory = "⏏"). Async load state → SkeletonRow.
- [x] **F6.12 — AppearancesSettingsScreen** — Already uses ListInputRow for launcher/desktop/font/statusbar cycling.

#### P2 — Occasional

- [x] **F6.13 — AboutScreen** — ListContainer + ListSection + ListItemRow; AboutModal uses Dialog widget.
- [x] **F6.14 — ControlsScreen** — ListContainer + ListSection per category (Board/Actions/Gestures) + ListItemRow already in use.
- [x] **F6.15 — DeveloperScreen** — ConfirmModal (danger dialog) before Stop Aether and Reboot to Bootloader.
- [x] **F6.16 — DolphinDemoScreen** — Footer hint now uses hintFor(Prev/Next/Activate/Back).
- [x] **F6.17 — SleepSettingsScreen** — Already uses ListInputRow for sleep interval.
- [ ] **F6.18 — SoundsSettingsScreen** — ListSection + ListItemRow already in use; Slider for volume deferred (needs AudioService volume API).

#### P3 — Sub-screens

- [x] **F6.19 — WifiNetworkDetailScreen** — ListContainer + ListItemRow for IP info + Forget action. Danger color on rows deferred (ListEntry.danger not yet a field).
- [x] **F6.20 — WifiIpConfigScreen** — DHCP toggle + IP field rows + VirtualKeyboard flow already implemented.
- [x] **F6.21 — BluetoothSettingsScreen** — Toggle + ListItemRow (device list, pair/confirm rows) already in use.
- [ ] **F6.22 — TouchSettingsScreen** — Placeholder screen (no touch driver API yet). Slider + Toggle deferred until touch driver exposes settings.
- [x] **F6.23 — CameraSettingsScreen** — Read-only device info, ListContainer + ListSection + ListItemRow (correct for read-only).
- [ ] **F6.24 — ProfileSettingsScreen** — TextField rows; pastikan VirtualKeyboard flow tetap benar.
- [x] **F6.25 — RemoteSettingsScreen** — Toggle + ListItemRow (connected device info, protocol rows) already correct.
- [x] **F6.26 — DesktopSettingScreen** — ListInputRow for fit mode selection (already implemented); 9-grid anchor picker is correct custom draw.
- [x] **F6.27 — FileTextViewerScreen** — ScrollView correct for raw text; line count now in header: "filename.txt (N lines)" / "N+ lines" when truncated.

#### P4 — Apps refactor minimal

- [x] **F6.28 — HelloApp** — Updated to ListContainer + ListSection + ListItemRow + Toggle; canonical "how to write an app" reference in comments.
- [x] **F6.29 — DolphinApp** — Footer hint now uses hintFor(Prev/Next/Activate/Back).

---

### Fase 6B — Feature Gaps (Ditemukan dari Audit Screen)

Fitur-fitur ini dibutuhkan oleh screen rewrites di atas tapi belum ada.

#### F6.A — Fullscreen App Mode

**Problem:** Sistem ada toggle status bar global (AppearanceSettings), tapi tidak ada
mekanisme per-app/per-screen untuk request fullscreen independent dari preferensi user.
Misalnya DolphinDemoScreen, game, atau media viewer harus bisa hide status bar saat
foreground dan restore saat background, tanpa mengubah setting user.

**Design:**

```cpp
// ComponentScreen (atau ComponentApp) tambah virtual method:
virtual bool wantsFullscreen() const { return false; }

// AetherServer / ViewDispatcher query ini sebelum render frame:
//   jika screen.wantsFullscreen() → skip statusbar draw untuk frame ini
//   jika tidak → ikut global preference
```

Implementasi: `AetherServer::render()` sudah tahu screen aktif. Tambah query
`currentScreen()->wantsFullscreen()` sebelum `statusBar_->draw()`. Tidak perlu
state baru di screen stack.

Tasks:
- [x] **F6.A1** — `fullscreen()` virtual already on ComponentScreen; DolphinDemoScreen + DolphinApp now override it
- [x] **F6.A2** — AetherServer already skips status bar for ScreenMode::Fullscreen (existing)
- [x] **F6.A3** — ScreenMode::Fullscreen propagated via s->mode() (existing mechanism)
- [x] **F6.A4** — `bool fullscreen() const override { return true; }` added to DolphinDemoScreen, DolphinApp

#### F6.B — AppList & FileBrowser Sorting

**Problem:** AppListScreen tidak sort entries; FileBrowserScreen sort bergantung pada
filesystem order (alphabetical di LittleFS tapi tidak di FAT32).

**Design:** LazyDirLoader sudah melakukan dirs-first + alphabetical sort di background
thread setelah `IFileSystem::list()` selesai. Saat AppListScreen dan FileBrowserScreen
direfactor ke LazyDirLoader (F6.04, F6.05), sorting ini otomatis dapat.

Untuk AppList yang support folder nested:
- Item yang `isDir=true` → render sebagai drilldown folder (ListItemRow + chevron)
- Aktivate folder → push screen baru dengan path baru ke LazyDirLoader
- Breadcrumb path di TitleBar

Tasks:
- [ ] **F6.B1** — LazyDirLoader sudah sort dirs-first alpha; verifikasi unit test di firmware/tests/
- [ ] **F6.B2** — AppListScreen: tambah folder drilldown (path stack, push screen per folder)
- [ ] **F6.B3** — FileBrowserScreen: ganti listing ke LazyDirLoader (sudah dapat sorting gratis)
- [ ] **F6.B4** — TitleBar: tampilkan current path / breadcrumb singkat saat dalam subfolder

#### F6.C — Modal & Dialog Polish

**Problem:** Dialog widget ada tapi ukurannya tidak responsif, tombol bisa overflow di
layar sempit, dan tidak ada "danger" styling untuk destructive actions.

Tasks:
- [x] **F6.C1** — `Dialog`: auto-size height (SIZE_AUTO, minH=40, maxH=90)
- [x] **F6.C2** — `DialogButton`: `bool danger = false` → `background=true` on button node
- [x] **F6.C3** — Modal render positioned in ComponentScreen::draw() for correct layout in box
- [ ] **F6.C4** — `PermissionScreen`: redesign ke Dialog widget bukan manual layout

#### F6.D — Display Rotation

**Problem:** Semua rendering diasumsikan portrait 0°. User yang pasang board landscape
(misal diputar 90°) tidak ada mekanisme software rotation. Ini penting untuk boards
yang bisa di-mount di berbagai orientasi.

**Design:**

Rotation di level `AetherServer` setelah layout+render selesai, sebelum blitting ke
IDisplay. AetherServer punya Canvas internal; setelah `renderer_.render()` ke canvas,
rotate pixel buffer sebelum `display_.flush()`.

```
Orientation: 0° (normal) | 90° CW | 180° | 270° CW (= 90° CCW)
```

Cara rotate 1-bit framebuffer:
- 0° → direct blit
- 90°/270° → transpose + flip (dst W/H swap)
- 180° → reverse bit order seluruh buffer

Setting disimpan di NVS (`display.rotation`) via SystemConfig. Berlaku global untuk
semua screens; TIDAK per-app (rotation adalah hardware mount property, bukan app UX).

Setelah rotation di-apply: layout engine tetap bekerja di logical coordinates
(width x height dalam orientasi normal). `canvas.width()`/`canvas.height()` yang
diexpose ke screen harus reflect **post-rotation** logical dimensions, bukan physical.

Tasks:
- [ ] **F6.D1** — `IDisplay` tambah `Orientation` enum (Rot0 / Rot90 / Rot180 / Rot270) + `setOrientation()` virtual method
- [ ] **F6.D2** — `AetherServer`: setelah `renderer_.render()`, rotate pixel buffer sesuai orientation sebelum blit ke display
- [ ] **F6.D3** — `canvas.width()`/`height()` reflect logical (rotated) size agar screens draw resolution-independent
- [ ] **F6.D4** — `SystemConfig` / NVS: simpan dan load `display.rotation`; default `Rot0`
- [ ] **F6.D5** — `AppearancesSettingsScreen`: tambah ListInputRow untuk Rotation (Normal / 90° / 180° / 270°)
- [ ] **F6.D6** — Hardware platform (ESP32 LCD driver): implement `setOrientation()` via MADCTL register atau software rotate buffer

---

### Fase 6C — Cleanup: Deprecated Components

Setelah Fase 6 screen rewrites selesai, component lama yang tidak lagi dipakai bisa dihapus:

| Component | File | Status | Alasan hapus |
|---|---|---|---|
| `ListRow()` | `widgets.h/.cpp` | **Superseded** oleh `ListItemRow` | Tidak ada fitur icon/chevron; hanya dipakai `HelloApp` (akan di-update F6.28) |
| `ListItem()` | `widgets.h/.cpp` | **Superseded** oleh `ListItemRow` | Sama dengan ListRow; label+accessory tanpa icon support |
| `Button()` | `widgets.h/.cpp` | **Keep** | Masih berguna untuk confirm dialogs; `ListItemRow` bukan penggantinya |
| Raw `ScrollView` | `widgets.h/.cpp` | **Keep, deprecate internally** | Masih dipakai FileTextViewerScreen + LogsScreen untuk non-list content |

Tasks:
- [ ] **F6.C5** — Setelah F6.28 (HelloApp update), hapus `ListRow` + `ListItem` dari `widgets.h/.cpp`
- [ ] **F6.C6** — Grep seluruh codebase untuk sisa pemakai `ListRow`/`ListItem`; jika nol → hapus
- [ ] **F6.C7** — Tambah `[[deprecated]]` attribute ke `ListRow`/`ListItem` SEBELUM dihapus (satu release grace period)

---

### Docs
- [ ] Update `docs/feats/` untuk Aether UI system setelah Fase 1 selesai
- [ ] Architecture doc: `docs/architecture/aether-ui.md`
- [ ] Migration guide untuk breaking changes (padding setter, namespace)
- [ ] Tambah "How to write a screen" guide dengan HelloApp (F6.28) sebagai contoh canonical
