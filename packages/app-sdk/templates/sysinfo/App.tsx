import { View, Text, Pressable, ScrollView, useState } from "nema";

// Example: uses the `nema` system API (device / storage / log) + a scrollable
// capability list. Tap count persists across launches via nema.storage.
export default function App() {
  const [taps, setTaps] = useState(Number(nema.storage.get("taps") || "0"));

  const bump = () => {
    const n = taps + 1;
    setTaps(n);
    nema.storage.set("taps", String(n));
    nema.log("info", "SysInfo", "tap " + n);
  };

  return (
    <View style={{ flexDirection: "column", padding: 3, gap: 2 }}>
      <Text variant="title">{nema.device.name}</Text>
      <Pressable onPress={bump}><Text>{`Taps: ${taps}  (tap +)`}</Text></Pressable>
      <Text variant="caption">Capabilities:</Text>
      <ScrollView style={{ flexGrow: 1 }}>
        {nema.device.caps.map((c) => <Text>{"- " + c}</Text>)}
      </ScrollView>
    </View>
  );
}
