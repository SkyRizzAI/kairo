import { View, Text, Pressable, Row, useState, useEffect } from "nema";

const FILE = "count.txt";

export default function App() {
  const [n, setN] = useState(0);
  const [loaded, setLoaded] = useState(false);

  // Load persisted count on first render.
  useEffect(() => {
    const saved = nema.storage.fs.readFile(FILE);
    if (saved !== null) {
      const parsed = parseInt(saved, 10);
      if (!isNaN(parsed)) setN(parsed);
    }
    setLoaded(true);
  }, []);

  // Persist every time count changes (skip initial unloaded state).
  useEffect(() => {
    if (!loaded) return;
    nema.storage.fs.writeFile(FILE, String(n));
  }, [n, loaded]);

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
