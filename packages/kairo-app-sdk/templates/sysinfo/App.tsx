import { View, Text, Pressable, ScrollView, useState } from "kairo";

// Example: uses the `kairo` system API (device / storage / log) + a scrollable
// capability list. Tap count persists across launches via kairo.storage.
export default function App() {
  const [taps, setTaps] = useState(Number(kairo.storage.get("taps") || "0"));

  const bump = () => {
    const n = taps + 1;
    setTaps(n);
    kairo.storage.set("taps", String(n));
    kairo.log("info", "SysInfo", "tap " + n);
  };

  return (
    <View style={{ flexDirection: "column", padding: 3, gap: 2 }}>
      <Text variant="title">{kairo.device.name}</Text>
      <Pressable onPress={bump}><Text>{`Taps: ${taps}  (tap +)`}</Text></Pressable>
      <Text variant="caption">Capabilities:</Text>
      <ScrollView style={{ flexGrow: 1 }}>
        {kairo.device.caps.map((c) => <Text>{"- " + c}</Text>)}
      </ScrollView>
    </View>
  );
}
