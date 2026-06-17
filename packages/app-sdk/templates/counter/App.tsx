import { View, Text, Pressable, Row, useState } from "nema";

// Example custom app — written in TSX, built to a single .kapp, loaded on device.
export default function App() {
  const [n, setN] = useState(0);

  return (
    <View style={{ flexDirection: "column", padding: 4, gap: 6, alignItems: "center" }}>
      <Text variant="title">{`Count: ${n}`}</Text>
      <Row style={{ gap: 8 }}>
        <Pressable onPress={() => setN((p) => p - 1)}>
          <Text>-</Text>
        </Pressable>
        <Pressable onPress={() => setN((p) => p + 1)}>
          <Text>+</Text>
        </Pressable>
      </Row>
      <Pressable onPress={() => setN(0)}>
        <Text>Reset</Text>
      </Pressable>
    </View>
  );
}
