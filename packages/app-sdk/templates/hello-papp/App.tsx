import { View, Text, Pressable, ScrollView, useState } from "nema";

// Hello Papp — example custom app (Plan 57/59).
// Demonstrates: nema.device, nema.storage (persist), nema.log.
// Install: drop hello-papp.papp into /flash/apps/, then `papp-scan`.
// Run from CLI: just type `hello-papp` (auto-detected via PATH=/apps).

export default function App() {
  const [count, setCount] = useState(Number(nema.storage.get("count") || "0"));
  const [msg, setMsg]     = useState("");

  const increment = () => {
    const n = count + 1;
    setCount(n);
    nema.storage.set("count", String(n));
    nema.log("info", "HelloPapp", `count → ${n}`);
    setMsg(`Tapped ${n} time${n !== 1 ? "s" : ""}`);
  };

  return (
    <View style={{ flexDirection: "column", padding: 3, gap: 2 }}>
      <Text variant="title">Hello Papp v1.0</Text>
      <Text variant="subtitle">Custom App Demo</Text>

      <View style={{ flexDirection: "row", gap: 4, alignItems: "center" }}>
        <Text>{`Device: ${nema.device.name}`}</Text>
      </View>
      <Text variant="caption">{`Capabilities: ${nema.device.caps.length} total`}</Text>

      <Pressable onPress={increment}>
        <View style={{ padding: 2, border: 1, alignItems: "center" }}>
          <Text>{msg || `Taps: ${count} (press +)`}</Text>
        </View>
      </Pressable>

      <Text variant="caption">Install via: papp-scan</Text>
      <Text variant="caption">Run via CLI: hello-papp</Text>
    </View>
  );
}
