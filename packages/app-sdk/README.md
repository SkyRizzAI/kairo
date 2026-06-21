# nema — custom-app SDK

Write Palanu device apps in **TSX** (React/Ink-style), build to a `.papp` bundle,
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

`manifest.json`:
```json
{ "id": "com.you.counter", "name": "Counter", "version": "1.0.0", "entry": "App.tsx", "needs": ["http"] }
```

## Build

```bash
bun bin/build.ts path/to/app             # → dist/<id>.papp/ (folder: manifest.json + app.js)
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
- **Embedded** (now): apps compiled into the firmware (the built-in store) via the
  generated `firmware/core/include/nema/apps/embedded_apps.h`.
- **Installed / OTA**: push a `.papp` from Palanu Forge over BLE/USB/WebSocket — the
  device installs it live into the launcher (volatile; or persisted to `/apps/`).

## Examples
- `templates/counter` — useState + buttons.
- `templates/sysinfo` — `nema.device`/`storage`/`log` + a scrollable caps list.
