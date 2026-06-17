/** @jsxImportSource nema */
import { View, Text } from "nema";

// Deauth — WiFi tools demo (stub).
// Build: bun build App.tsx --outdir . --minify --external nema --external "nema/*"
// Install: copy manifest.json + app.js to /apps/deauth.papp/

export default function App() {
  const caps = nema.device.caps;

  return (
    <View style={{ flexDirection: "column", padding: 3, gap: 2 }}>
      <Text variant="title">WiFi Tools</Text>
      <Text variant="caption">Deauth scanner (coming soon)</Text>
      <Text>{`WiFi available: ${caps.includes("net.wifi") ? "YES" : "NO"}`}</Text>
      <Text>{`Capabilities: ${caps.length} total`}</Text>
    </View>
  );
}
