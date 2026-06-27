// Web3 Test — a demo dApp for the on-device wallet (Plan 94). List-driven UX matching the
// built-in Settings / Wallets screens: a home menu + a network-picker sub-screen. The rows
// are borderless; the focused row gets the system's rounded selection highlight. Works across
// chains — pick an EVM, Bitcoin or Solana network, read the address, or sign a message/tx.
//
// The wallet API returns the value on success and THROWS on failure; signing pops the system's
// trusted-display consent — the private key never leaves the device (sealed by the SE050 on
// skyrizz-e32).
import { View, Text, Pressable, ScrollView, useState } from "nema";

const NETWORKS: string[] = nema.wallet.networks();

function chainOf(id: string): string {
  if (id.indexOf("btc") === 0) return "Bitcoin";
  if (id.indexOf("sol") === 0) return "Solana";
  return "EVM";
}

// A throwaway unsigned EVM (EIP-155, chainId 1) transfer for the "Sign tx" demo.
const DEMO_EVM_TX =
  "e4808504a817c8008252089400000000000000000000000000000000000000008080018080";

// One borderless menu row — label left, optional hint right. The focused row is highlighted
// by the system (selectBox), so no per-row border boxes.
const row = {
  padding: 3,
  gap: 4,
  flexDirection: "row" as const,
  justifyContent: "space-between" as const,
  alignItems: "center" as const,
};

export default function App() {
  const [screen, setScreen] = useState("home");
  const [net, setNet] = useState(NETWORKS[0] || "eth-mainnet");
  const [out, setOut] = useState("");

  const ensure = (cap: string) => nema.sys.perm.request(cap) === 1;

  const showAddress = () => {
    if (!ensure("wallet.read")) return setOut("Permission denied");
    try { setOut(nema.wallet.address(net, 0)); }
    catch (e) { setOut("Error: " + e); }
  };
  const signMessage = () => {
    if (!ensure("wallet.sign")) return setOut("Permission denied");
    try { setOut("sig " + nema.wallet.signMessage(net, 0, "Hello from Palanu").slice(0, 28) + "…"); }
    catch (e) { setOut("Error: " + e); }
  };
  const signTx = () => {
    if (chainOf(net) !== "EVM") return setOut("Tx demo is EVM-only — use Sign message for " + chainOf(net));
    if (!ensure("wallet.sign")) return setOut("Permission denied");
    try { setOut("tx " + nema.wallet.signTransaction(net, 0, DEMO_EVM_TX).slice(0, 28) + "…"); }
    catch (e) { setOut("Error: " + e); }
  };

  // ── Sub-screen: pick a network ──
  if (screen === "net") {
    return (
      <View style={{ flexDirection: "column", padding: 2 }}>
        <Text variant="caption">SELECT NETWORK</Text>
        <ScrollView style={{ flexGrow: 1 }}>
          <Pressable onPress={() => setScreen("home")} style={row}>
            <Text>‹ Back</Text>
          </Pressable>
          {NETWORKS.map((n) => (
            <Pressable key={n} onPress={() => { setNet(n); setScreen("home"); }} style={row}>
              <Text>{(n === net ? "● " : "  ") + n}</Text>
              <Text variant="caption">{chainOf(n)}</Text>
            </Pressable>
          ))}
        </ScrollView>
      </View>
    );
  }

  // ── Home: action menu ──
  return (
    <View style={{ flexDirection: "column", padding: 2 }}>
      <Text variant="title">Web3 Test</Text>
      <Text variant="caption">{net + " · " + chainOf(net)}</Text>
      <ScrollView style={{ flexGrow: 1 }}>
        <Pressable onPress={() => setScreen("net")} style={row}>
          <Text>Network</Text>
          <Text variant="caption">{net + " ›"}</Text>
        </Pressable>
        <Pressable onPress={showAddress} style={row}>
          <Text>Show address</Text>
        </Pressable>
        <Pressable onPress={signMessage} style={row}>
          <Text>Sign message</Text>
          <Text variant="caption">{chainOf(net)}</Text>
        </Pressable>
        <Pressable onPress={signTx} style={row}>
          <Text>Sign transaction</Text>
          <Text variant="caption">{chainOf(net) === "EVM" ? "EVM" : "EVM only"}</Text>
        </Pressable>
      </ScrollView>
      <Text variant="caption">{out || "Pick an action"}</Text>
    </View>
  );
}
