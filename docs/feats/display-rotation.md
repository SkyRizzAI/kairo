# Display Rotation

> Turn the whole UI 0°/90°/180°/270° from Settings — the screen, touch, and directional
> buttons all follow, on a board-agnostic seam that degrades to a fixed boot-time rotation.

## What the user sees

- **Settings → Display → Rotation** is a cycling selector with values `0` / `90` / `180` / `270`
  (`sleep_settings_screen.cpp:25`, the `kRotLabels` array; rendered as the `"Rotation"` row in
  `build()` at `sleep_settings_screen.cpp:148`).
- Adjusting it rotates the UI **live** on drivers that support it (simulator and the skyrizz
  hardware): the screen reflows, touch input lands in the right place, and the device's physical
  directional buttons remap so "up" still feels like up after you turn the device in your hand.
- The choice is **persisted** to config key `display`/`rotation` (`sleep_settings_screen.cpp:81`),
  so it survives reboot. On a driver with no live-rotation support the persisted value simply
  applies at next boot.
- `90`/`270` are landscape: the logical resolution swaps (e.g. 240×320 → 320×240), and because the
  whole UI draws from `canvas.width()`/`height()` it lays itself out for the new aspect with no
  per-screen changes.

## How it works

### The board-agnostic seam

Rotation is exposed through two virtual hooks, both defaulting to no-ops so a board opts in:

- `IDisplayDriver::setRotation(uint8_t)` / `rotation()` — `display.h:53-54`. Default no-op; a driver
  that overrides `setRotation` advertises live rotation. `90`/`270` swap `width()`/`height()`.
- `IKeyMap::setRotation(uint8_t)` — `i_key_map.h:43`. Default no-op; boards with directional buttons
  override it to remap them per orientation.

`SleepSettingsScreen::cycleRotation()` (`sleep_settings_screen.cpp:79`) is the single driver of all
of this. It writes the config, then:
1. resolves `IDisplayDriver` and calls `setRotation()` (`sleep_settings_screen.cpp:85`),
2. resolves `ITouchDriver` and calls its `setRotation()` (`sleep_settings_screen.cpp:86`),
3. publishes a `DisplayRotationChanged` event carrying the new `rotation` value
   (`sleep_settings_screen.cpp:89`; event name in `event.h:30`).

The same value is **also read at init** by each driver, so a no-live driver still rotates at boot.

### skyrizz hardware — LCD (ILI9341)

`LcdDriver::init()` reads `display`/`rotation` from config and, for `90`/`270`, swaps the logical
`width_`/`height_` while keeping `nativeW_`/`nativeH_` as the portrait native dims
(`lcd_driver.cpp:43-54`). The 1-bit framebuffer byte size is `w*h`, which is identical either way, so
`start()` allocates the same buffer regardless of orientation.

Orientation reaches the panel via MADCTL (command `0x36`). `applyMadctl()` indexes a 4-entry table
`{0x48, 0x28, 0x88, 0xE8}` for `0/90/180/270` (`lcd_driver.cpp:175-180`); the BGR bit `0x08` is kept
in every value so `blitRgb565()`'s R↔B swap stays valid. `panelInit()` sends it at boot
(`lcd_driver.cpp:153`).

Live rotation: `LcdDriver::setRotation()` (`lcd_driver.cpp:187-194`) re-swaps the logical dims from
`nativeW_/nativeH_`, re-sends MADCTL so the panel re-scans, and forces a full repaint
(`fullFlush_ = true`). The live path is marked bench-untested in the source comment; the boot-time
config path is the verified one.

### skyrizz hardware — touch (FT6336U)

The FT6336U panel is native portrait 240×320; `LCD_W`/`LCD_H` are the **physical**,
rotation-invariant touch dimensions (`ft6336_touch.cpp:18-19`). `Ft6336Touch::init()` reads the
same `display`/`rotation` config (`ft6336_touch.cpp:24-25`). `toLogical()`
(`ft6336_touch.cpp:98-108`) maps raw → logical with a 4-case transform paired to the MADCTL table:

- `0°`:  `lx=rawX, ly=rawY`
- `90°`: `lx=rawY, ly=(LCD_W-1)-rawX`
- `180°`: `lx=(LCD_W-1)-rawX, ly=(LCD_H-1)-rawY`
- `270°`: `lx=(LCD_H-1)-rawY, ly=rawX`

The source notes the on-device caveat: if one orientation reads mirrored on real hardware, flip the
sign in just that case — components never see raw values (`ft6336_touch.cpp:96-97`).

### skyrizz hardware — directional buttons

`E32KeyMap::rotateId()` (`e32_key_map.cpp:39-45`) rotates the 4 directional buttons around the ring
`{Up, Right, Down, Left}`, stepping `-rotation_` (the opposite way) because the panel rotates CCW
relative to the buttons on this board — verified on hardware (at 90° the Right button drives Up).
MIDDLE/OK is orientation-independent. It's applied in `onGesture()` before code/action lookup
(`e32_key_map.cpp:28`).

The board wires it up in `skyrizz_e32.cpp`: it seeds `keyMap_.setRotation()` from persisted config at
init (`skyrizz_e32.cpp:50-51`) and subscribes to `DisplayRotationChanged` to follow live changes
(`skyrizz_e32.cpp:52-56`).

### Simulator (WASM)

There is no panel glass, so `NullDisplay::setRotation()` (`null_display.h:30-35`) just swaps the
reported logical dims `w_`/`h_` from `nativeW_`/`nativeH_` (no MADCTL). The UI reflows, and because
pointer events come back from Forge already in the displayed (rotated) space, **no touch transform**
is needed.

`RemoteScreenTap` streams the buffer to Forge. `ensureShadow()` (`remote_screen_tap.cpp:12-19`)
resizes the shadow buffer whenever the inner driver's dims change; it's re-checked at frame start in
`clear()` (`remote_screen_tap.cpp:33-39`) and in `flushBuffer()` (`remote_screen_tap.cpp:54-60`), so
a live dim-swap is picked up before the frame's pixels land. The streamed payload carries the current
`w_`/`h_` header (`remote_screen_tap.cpp:76-81`), and Forge adapts the mirror.
