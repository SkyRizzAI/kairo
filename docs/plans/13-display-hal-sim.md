# 13 — Display HAL + SimDisplay + Web Display Panel

> 1-bit monochrome canvas, resolusi 264×176 (persis e-ink hardware ref). SimDisplay render ke buffer → JSON base64 → browser canvas. Event-driven: flush hanya saat konten berubah.

- Status: ☐ Not started
- Milestone: M5 (UI Runtime) — prasyarat
- Depends on: 06 (HAL pattern), 09 (bridge protocol), 10 (web UI)
- Blocks: 14 (Canvas + Screen system), 18 (ESP32 e-ink driver)

---

## Goal

- `IDisplayDriver`: 1-bit interface — drawPixel(x, y, bool), fillRect, flush, clear.
- `Canvas`: abstraksi drawing 1-bit + bitmap font. **Ini yang dipakai UI Runtime, bukan driver langsung.**
- `SimDisplay`: buffer `uint8_t[264*176]` (1 byte/pixel, mudah dimanipulasi) → flush emits `{"type":"frame"}`.
- Web panel **Display**: canvas HTML 264×176 di-scale 3× (`pixelated`) = 792×528 visual. Default skin: putih kertas + hitam tinta (e-ink accurate). Toggle ke retro green phosphor.

## Scope

### In scope

- `IDisplayDriver` (1-bit: drawPixel bool, fillRect, clear, flush).
- `Canvas` (1-bit drawing layer: drawPixel, fillRect, drawLine, drawText, drawBitmap, invertRect, setFont).
- `BitmapFont` (5×8 pixel font, embedded const array — cukup ASCII 0x20–0x7E).
- `SimDisplay` (buffer + flush → emit frame JSON ke TelemetryBridge).
- `DisplayManager` menjadi tipis: hanya menyimpan `IDisplayDriver&` + expose `Canvas&`.
- Web: `DisplayPanel.tsx` (canvas 264×176, scale 3×, decode 1-bit buffer, theme switcher).
- Platform + Board: register SimDisplay, tambah capability `"display"`.

### Out of scope

- Color rendering (Kairo UI adalah 1-bit — bahkan di LCD color, pakai monochrome palette).
- Partial refresh / dirty-rect optimization (implementasikan saat dibutuhkan di hardware e-ink).
- Touch input.
- Anti-aliasing, truetype font.

---

## Design

### Resolusi & buffer

```
Width  = 264 px
Height = 176 px
Buffer = uint8_t[264 * 176] = 46 464 bytes (~45 KB)
```

Satu byte per pixel (bukan packed bit) untuk kemudahan drawing. Flush ke hardware bisa pack di sisi driver.

### IDisplayDriver (1-bit)

```cpp
// firmware/core/include/kairo/hal/display.h
namespace kairo {

struct IDisplayDriver : IDriver {
    virtual uint16_t width()  const = 0;
    virtual uint16_t height() const = 0;
    virtual void drawPixel(uint16_t x, uint16_t y, bool on) = 0;
    virtual void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) = 0;
    virtual void clear(bool on = false) = 0;
    virtual void flush() = 0;     // push buffer ke panel / emit frame JSON
};

}
```

### Canvas

Canvas adalah layer drawing yang UI Runtime pakai. Tidak tahu tipe driver.

```cpp
// firmware/core/include/kairo/ui/canvas.h
namespace kairo {

struct BitmapFont {
    const uint8_t* data;    // glyphs packed: [charW * charH bits per glyph]
    uint8_t charW, charH;   // e.g. 5, 8
    uint8_t firstChar;      // biasanya 0x20 (space)
    uint8_t numChars;
};

extern const BitmapFont FONT_5X8;   // default pixel font

class Canvas {
public:
    Canvas(IDisplayDriver& driver);

    uint16_t width()  const;
    uint16_t height() const;

    void clear(bool on = false);
    void drawPixel(uint16_t x, uint16_t y, bool on = true);
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on = true);
    void drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on = true);  // outline
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, bool on = true);
    void drawText(uint16_t x, uint16_t y, const char* text, bool on = true);
    void drawBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* bits);
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);  // untuk cursor highlight

    void setFont(const BitmapFont& font);
    uint16_t textWidth(const char* text) const;  // buat centering

    void flush();  // delegates to driver.flush()

private:
    IDisplayDriver& driver_;
    const BitmapFont* font_ = &FONT_5X8;
};
}
```

### BitmapFont 5×8

Font 5×8 pixel klasik (turunan CP437 / classic PC font). ASCII 0x20–0x7E (95 karakter).

```
264 / 6 = 44 kolom karakter  (5px char + 1px spacing)
176 / 9 = 19 baris karakter  (8px char + 1px spacing)
```

Tersedia sebagai `extern const uint8_t FONT_5X8_DATA[]` di `src/ui/font_5x8.cpp`.

### SimDisplay

```cpp
// firmware/platforms/simulator/include/kairo/sim/sim_display.h
class SimDisplay : public IDisplayDriver, public IService {
    static constexpr uint16_t W = 264, H = 176;
    uint8_t buf_[W * H] = {};   // 0=black, 1=white
    Logger* log_ = nullptr;
    IClock* clock_ = nullptr;
    // bridge untuk emit frame
public:
    void init(Logger& log, IClock& clock, TelemetryBridge& bridge);

    uint16_t width()  const override { return W; }
    uint16_t height() const override { return H; }
    const char* name() const override { return "SimDisplay"; }
    DriverKind  kind() const override { return DriverKind::Display; }

    void drawPixel(uint16_t x, uint16_t y, bool on) override;
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void clear(bool on) override;
    void flush() override;   // → emit {"type":"frame",...}

    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}
};
```

### Protokol frame

`flush()` emit satu JSON line ke stdout:

```json
{
  "type":     "frame",
  "ts":       1234567890,
  "width":    264,
  "height":   176,
  "encoding": "1bit_rle",
  "data":     "<base64 of run-length encoded buffer>"
}
```

Encoding RLE sederhana: pasangan `[count, value]` bytes → compress run putih/hitam yang panjang.
Alternatif fallback: `"encoding":"raw_base64"` untuk debugging (46 KB per frame → base64 ~62 KB).

> Implementasi pertama: `raw_base64` (simpler). Optimasi ke RLE kalau frame terlalu besar/lambat.

### Web: DisplayPanel.tsx

```tsx
// 264×176 canvas, scale 3× via CSS transform
// imageRendering: pixelated  ← penting agar tidak blur saat scale
// Default skin: putih (#F0EDE0 paper) + hitam (#1a1a1a ink)
// Toggle: 🟢 green phosphor (#0f0f0f bg + #33ff33 px)

function DisplayPanel({ frame }: { frame: FrameMsg | null }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    useEffect(() => {
        if (!frame || !canvasRef.current) return;
        const ctx = canvasRef.current.getContext("2d")!;
        const buf = atob(frame.data);   // raw_base64
        const imgData = ctx.createImageData(264, 176);
        for (let i = 0; i < 264*176; i++) {
            const on = buf.charCodeAt(i) > 0;
            const c = on ? 255 : 0;
            imgData.data[i*4+0] = c; // R
            imgData.data[i*4+1] = c; // G
            imgData.data[i*4+2] = c; // B
            imgData.data[i*4+3] = 255;
        }
        ctx.putImageData(imgData, 0, 0);
    }, [frame]);
    return (
        <canvas ref={canvasRef} width={264} height={176}
            style={{ imageRendering:"pixelated", width:528, height:352, border:"1px solid #333" }} />
    );
}
```

Scale 2× (528×352) lebih compact dari 3× untuk layout panel. Adjustable.

### Layout web: tambah panel Display

Panel Display ditambah sebagai panel ke-5. Karena penting (visual utama), tempatkan di **kiri atas** atau **tengah atas**, geser panels lain ke bawah/kanan.

Revisi layout grid di `frontend.tsx`:
```
┌─────────────────┬──────────────────┐
│  Display 264×176│  Logs            │
│  (scale 2×)     │                  │
├─────────────────┼──────────────────┤
│  Services       │  Events          │
├─────────────────┼──────────────────┤
│  Controls       │                  │
└─────────────────┴──────────────────┘
```

---

## Tasks

- [ ] `firmware/core/include/kairo/hal/display.h` (`IDisplayDriver` 1-bit).
- [ ] `Canvas` + `BitmapFont` struct di `firmware/core/include/kairo/ui/canvas.h`.
- [ ] `firmware/core/src/ui/font_5x8.cpp` — data font 5×8, 95 glyphs.
- [ ] `firmware/core/src/ui/canvas.cpp` — drawPixel, fillRect, drawRect, drawLine, drawText, drawBitmap, invertRect, flush.
- [ ] `SimDisplay` (buf + drawPixel/fillRect/clear/flush → emit frame).
- [ ] `TelemetryBridge`: handle `{"type":"frame"}` pass-through + send di flush.
- [ ] `SimulatorPlatform::registerDrivers`: tambah SimDisplay.
- [ ] `Runtime::canvas()` accessor (buat Canvas dari active driver).
- [ ] `packages/simulator/lib/useSimSocket.ts`: tambah `frame: FrameMsg | null` ke SimState.
- [ ] `packages/simulator/components/DisplayPanel.tsx`.
- [ ] Revisi layout grid `frontend.tsx`.
- [ ] Core CMakeLists: tambah `src/ui/canvas.cpp`, `src/ui/font_5x8.cpp`.
- [ ] Platform CMakeLists: tambah `src/sim_display.cpp`.
- [ ] Verifikasi: `canvas.fillRect(0,0,264,176,false); canvas.drawText(10,80,"KAIRO",true); canvas.flush();` → terlihat di browser.

## Acceptance criteria

- Canvas 264×176 muncul di browser dengan background hitam/putih.
- `drawText("HELLO KAIRO", 10, 80)` dengan font 5×8 → teks terbaca di canvas.
- `invertRect` pada area teks → efek highlight terbalik.
- `flush()` di C++ → frame update di browser dalam <200ms (localhost).
- Core tidak tahu resolusi atau tipe driver — hanya pakai `IDisplayDriver` interface.

## Risks / notes

- Font 5×8 data: bisa generate dari public domain bitmap font (misalnya Terminus, atau classic IBM PC 5×8). Sertakan lisensi.
- `raw_base64` frame = ~62KB per flush. Untuk development localhost ini fine. Kalau terasa lambat, implement RLE.
- `imageRendering: pixelated` CSS wajib agar scale tidak blur. Tanpa ini pixel art jadi smooth/blurry.
- Pada hardware e-ink nyata, `flush()` akan panggil `display.display()` GxEPD2 — full refresh ~1s. Partial refresh untuk cursor update. Arsitektur ini sudah siap untuk itu.
