# 33 — Component Model: TSX/JS Mapping & Styling

> Memastikan sistem komponen native (Plan 27/30/31) **fleksibel & 1:1 dengan
> React TSX**, supaya custom app ber-engine JS nanti merender pohon yang SAMA
> seperti app C. Plus: bagaimana styling & kustomisasi dilakukan.

Status: **DESIGN** (model native sudah selaras; bridge JS = Plan 27 Fase 6).

---

## 1. Prinsip: satu pohon, dua produsen

`UiNode` adalah satu-satunya representasi UI. Hari ini diproduksi oleh **builder
C** (`widgets.h`). Nanti diproduksi juga oleh **reconciler JS** dari JSX. Renderer,
layout, focus, gesture, scroll — semuanya bekerja di `UiNode`, tidak peduli
siapa yang membangunnya. (Sudah dicatat di `node.h`.)

```
  C:   View(a, style, { Text(a,"Hi") })
  TSX: <View style={...}><Text>Hi</Text></View>
            └──────────────┬──────────────┘
                  sama-sama → UiNode tree → layout/render
```

## 2. Mapping node ↔ JSX (sudah ada)

| UiNode (native)      | JSX                                   | Catatan |
|----------------------|----------------------------------------|---------|
| `NodeType::View`     | `<View style>`                         | flex container |
| `NodeType::Text`     | `<Text>`                               | `role` → `variant`/style |
| `NodeType::Pressable`| `<Pressable onPress>`                  | fokusabel + tap |
| `NodeType::Scroll`   | `<ScrollView>`                         | viewport dibatasi flex, overflow scroll |
| `NodeType::Slider`   | `<Slider value min max onChange/>`     | native (track/knob/drag) |
| `Row/Col/Container`  | `<View style={{flexDirection}}>`       | composite = fungsi |
| `Toggle/Stepper/Select/TextField` | komponen RN-style       | composite dari primitives |

**Composite = fungsi yang mengembalikan pohon.** Itu PERSIS model React: sebuah
komponen JS `function Settings() { return <View>… </View> }` setara dengan
builder C `UiNode* buildSettings(...)`. Jadi "custom component" sudah didukung
secara konseptual — tinggal engine JS-nya.

## 3. Styling: `Style` ↔ style object

`Style` sengaja **flat & POD** supaya bisa di-serialize lintas batas JS↔C dan
memetakan 1:1 ke style object JSX:

| `Style` field | JSX style key | Status |
|---|---|---|
| `dir` | `flexDirection: 'row'|'column'` | ✅ |
| `flexGrow` | `flexGrow` | ✅ |
| `width`/`height` | `width`/`height` (atau `SIZE_AUTO`) | ✅ |
| `padding` | `padding` (uniform) | ✅ (lihat ext.) |
| `gap` | `gap` | ✅ |
| `align` | `alignItems` | ✅ |
| `justify` | `justifyContent` | ✅ |
| `border`/`background` | `borderWidth`/`backgroundColor` (mono) | ✅ (boolean now) |

### Ekstensi styling yang direncanakan (low-churn, additive)
1. **Per-side spacing**: `paddingTop/Right/Bottom/Left` + `margin*`. Simpan
   sebagai 4×uint8 (tetap POD). Builder `padding(uint8)` jadi shorthand.
2. **Warna/tema**: board mono → `border/background` boolean cukup; board warna
   (RGB565) butuh `fgColor/bgColor` opsional. Tambah `uint16_t` opsional +
   flag "pakai tema".
3. **Wrap & posisi absolut**: `flexWrap`, `position:absolute` + `top/left` —
   hanya kalau ada kebutuhan; default tetap single-axis (murah di MCU).

## 4. Theming / kustomisasi tampilan

- **Token via `Theme`**: `TextRole`→font/scale sekarang di `fontForRole()`.
  Bungkus jadi `Theme { fontFor(role), spacing(token), color(token) }` yang bisa
  di-set per-app. Default = tema sistem. Custom app override `Theme` tanpa
  menyentuh layout.
- **Variant Text** (`Title/Body/Caption`) = analog `variant` di design system;
  app JS cukup `<Text variant="title">`.
- **Spacing scale**: `gap={2}` sekarang px mentah; bisa jadi token (`gap="sm"`).

## 5. Batas reconciler JS↔C (untuk Fase 6)

- **Node description serializable**: `{ type, style(POD), props(primitif), children[] }`.
  Tidak ada pointer yang menyeberang kecuali handler.
- **Handler = id, bukan pointer**: di JS `onPress` adalah fungsi → bridge simpan
  sebagai `handlerId`. `UiNode` untuk app JS membawa `handlerId`; saat ditekan,
  C memanggil balik ke JS via id. (Native tetap pakai function pointer.) →
  tambah `uint32_t handlerId` opsional di `UiNode`, atau union dengan `onPress`.
- **Arena tetap dipakai**: reconciler menulis ke `NodeArena` yang sama → zero
  per-frame heap, sama seperti app C.
- **State & reconciliation**: v1 cukup "rebuild tree tiap frame" (sudah jadi
  model kita; identik dengan immediate re-render). Diff/keys = optimisasi nanti.

## 6. Yang sudah aman sekarang (tidak perlu diubah)

- Pohon dibangun ulang tiap frame dari arena → cocok dengan render model React.
- Composite = fungsi → custom component sudah jalan (lihat `UiShowcaseApp`,
  `Toggle/Stepper/Select`).
- `Style` flat/POD → siap di-serialize.
- Input controls native (`Slider`) + composites (`Toggle/Stepper/Select/
  TextField`) → setara komponen form RN.

## 7. Urutan kerja saat engine JS dimulai (Fase 6)

1. `handlerId` di `UiNode` (union dgn `onPress`/`onChange`).
2. Serializer node-desc ↔ deserializer ke `NodeArena`.
3. Binding JSX intrinsics (`View/Text/Pressable/ScrollView/Slider`) → node-desc.
4. `Theme` injectable + token spacing/warna.
5. Per-side padding/margin (kalau layout butuh).
6. (opsional) diff/keys untuk hemat rebuild.

> Intinya: **model native hari ini sudah dirancang sebagai target TSX**. Yang
> tersisa murni "jembatan" JS→node-desc + handler-id + tema — bukan perombakan
> layout/render.
