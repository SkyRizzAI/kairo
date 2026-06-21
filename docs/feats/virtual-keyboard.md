# Virtual Keyboard

On-screen QWERTY keyboard used whenever a screen needs text input (WiFi
password, profile name, remote password, IP address fields).

## Visual design

Inspired by Flipper Zero's compact keyboard:

- **No key borders** — unselected keys show only their character label
- **Rounded fill** (`fillRoundRect r=2`) for the focused/selected key
- **Capped width** — maximum 210 px; centered on wider displays so it never
  stretches edge-to-edge on portrait or large screens
- **Keys flush** — cells tile with no gap between them
- **Bottom-anchored** — keyboard sits at the bottom of the screen; prompt and
  input field fill the space above
- Key height capped at 13 px so tall screens don't produce oversized keys

## Layout

```
Row 0  │ Q W E R T Y U I O P       (10 chars)
Row 1  │ A S D F G H J K L <-      (9 chars + backspace)
Row 2  │ ^ Z X C V B N M , .       (^ = caps in alpha mode; 10 chars in Num/Sym)
Row 3  │ [123] [   SPACE   ] [OK][X]  (mode 3-wide, space 4-wide, ok 2-wide, X 1-wide)
```

## Modes

| Mode | Row 3 button shows | Row 0 | Row 1 (9 keys) | Row 2 (10 keys) |
|---|---|---|---|---|
| **ABC** (upper) | `123` | `QWERTYUIOP` | `ASDFGHJKL` | `^` caps + `ZXCVBNM,.` |
| **abc** (lower) | `123` | `qwertyuiop` | `asdfghjkl` | `v` caps + `zxcvbnm,.` |
| **123** | `!@#` | `1234567890` | `!@#$%^&*(` | `)-_=+[]{};` |
| **!@#** (Sym) | `abc` | `` ~`|\\/<>?,. `` | `':;,.!@#$%` | `&*()+=^-_~` |

Mode cycles: **ABC/abc → 123 → !@# → abc** via the mode button in row 3.

Caps key (`^`/`v`) only appears in alpha modes (ABC/abc). Pressing it toggles
upper ↔ lower. The label itself shows current case: `^` = uppercase active,
`v` = lowercase active.

Backspace is always at row 1 col 9, labeled `<-`.

Action row:
- **X** — cancel (discard input, close keyboard)
- **OK** — confirm (save input, close keyboard)
- Hardware `Back` button = backspace while text is present; cancel on empty buffer

## Input routing

VirtualKeyboard has two navigation modes set by the caller before opening:

```cpp
kbd_.linear = !rt_.capabilities().has(caps::Input2D);
```

| `kbd_.linear` | Navigation | When |
|---|---|---|
| `false` (2D) | `onCode(c)` → `handle(keyFromCode(c))` — Up/Down/Left/Right map geometrically | devices with `caps::Input2D` (WASM sim, skyrizz-e32, dev-board) |
| `true` (linear) | `onAction(a)` → `handleAction(a)` — Prev/Next walk cursor linearly | minimal 3-button devices |

The WASM simulator declares `caps::Input2D` (Forge sends arrow-key codes), so
it always uses the 2D path.

Wrap-around is enabled in both axes: leftmost col → rightmost col on Left press;
top row → action row on Up press.

## Screens that use VirtualKeyboard

| Screen | Fields |
|---|---|
| `WifiSettingsScreen` | SSID (plain), password (masked) |
| `RemoteSettingsScreen` | Remote password (masked) |
| `ProfileSettingsScreen` | User name, device name (plain); password (masked) |
| `WifiIpConfigScreen` | IP, subnet, router, DNS (plain) |

All four follow the same pattern: `swallowCode_ = true` when opening (to absorb
the Enter keypress that triggered the keyboard), `onCode` for 2D navigation,
`onAction`+`handleAction` for linear fallback.
