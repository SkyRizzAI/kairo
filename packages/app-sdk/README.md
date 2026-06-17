# nema — custom-app SDK

Write Kairo device apps in **TSX** (React/Ink-style), build to a single `.kapp`,
load on the device. Apps run on the embedded QuickJS engine and render through the
**same native components** as built-in apps (Plan 37).

## Write an app

```tsx
import { View, Text, Pressable, useState } from "nema";

export default function App() {
  const [n, setN] = useState(0);
  return (
    <View style={{ flexDirection: "column", padding: 4, gap: 6, alignItems: "center" }}>
      <Text variant="title">{`Count: ${n}`}</Text>
      <Pressable onPress={() => setN(n + 1)}><Text>+1</Text></Pressable>
    </View>
  );
}
```

`kapp.json`:
```json
{ "id": "com.you.counter", "name": "Counter", "version": "1.0.0", "entry": "App.tsx", "needs": ["http"] }
```

## Build

```bash
bun bin/nema-build.ts path/to/app        # → <id>.kapp (single file)
```

## Components
`View`, `Text` (`variant`: body|title|caption), `Pressable` (`onPress`),
`ScrollView`, `Slider`, `Row`, `Col`. `style`: `flexDirection`, `flexGrow`,
`width`, `height`, `padding`, `gap`, `alignItems`, `justifyContent`,
`border`, `background`. Hooks: `useState`, `useRef`, `useEffect`.

## System API (`nema.*`, capability-gated)
```ts
nema.log(level, tag, msg);
nema.device.name; nema.device.caps; nema.device.has("wifi");
nema.storage.get(key); nema.storage.set(key, val); nema.storage.remove(key);  // per-app
await nema.http.get(url);   // { status, body } — present only if the board networks
```
A method is only present if the board supports it — guard with `nema.device.has(...)`.

## Loading
- **Embedded** (now): apps compiled into the firmware (the built-in store) — see
  `scripts/gen-embedded-apps.ts`.
- **Installed / OTA** (planned): push a `.kapp` from Kairo Forge over BLE/USB — needs
  the device filesystem layer first.

## Examples
- `templates/counter` — useState + buttons.
- `templates/sysinfo` — `nema.device`/`storage`/`log` + a scrollable caps list.
