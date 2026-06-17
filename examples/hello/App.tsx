/** @jsxImportSource nema */
import { View, Text, Pressable, useState } from "nema";

// Hello — minimal counter app.
// Demonstrates: nema.device, nema.storage (persist), nema.log.
// Build: bun build App.tsx --outdir . --minify --external nema --external "nema/*"
// Install: copy manifest.json + app.js to /apps/hello.papp/

export default function App() {
  const [count, setCount] = useState(Number(nema.storage.get("count") || "0"));

  const bump = () => {
    const n = count + 1;
    setCount(n);
    nema.storage.set("count", String(n));
    nema.log("info", "Hello", `tap ${n}`);
  };

  return (
    <View style={{ flexDirection: "column", padding: 3, gap: 2 }}>
      <Text variant="title">Hello Palanu</Text>
      <Text variant="caption">{`Device: ${nema.device.name}`}</Text>
      <Pressable onPress={bump}>
        <View style={{ padding: 2, border: 1, alignItems: "center" }}>
          <Text>{`Taps: ${count}`}</Text>
        </View>
      </Pressable>
    </View>
  );
}
