// Web3 Test — a demo custom app (dApp) for the on-device wallet (Plan 94, Fase 7).
//
// Shows the whole bridge without a phone/browser: pick a network, read the active
// wallet's address, and request a message- or transaction-signature. Signing pops the
// trusted-display consent (physical button) and NEVER exposes the private key — the app
// only gets the address + signature, exactly like MetaMask/Phantom talking to a wallet.
import { View, Text, Pressable, ScrollView, useState } from "nema";

// All network ids the device supports (eth-mainnet, sepolia, polygon, bnb, arbitrum,
// optimism, base, avalanche, btc-mainnet, btc-testnet, sol-mainnet, sol-devnet).
const NETWORKS: string[] = nema.wallet.networks();

// A throwaway unsigned EVM (EIP-155, chainId 1) transfer used by "Sign transaction".
// Only meaningful on EVM networks; BTC/SOL will report an error (good for testing).
const DEMO_EVM_TX =
  "e4808504a817c8008252089400000000000000000000000000000000000000008080018080";

export default function App() {
  const [i, setI] = useState(0);
  const [out, setOut] = useState("");

  const net = NETWORKS.length ? NETWORKS[i % NETWORKS.length] : "eth-mainnet";
  const cycle = () => setI((p) => (p + 1) % Math.max(NETWORKS.length, 1));

  // Permissions follow the system model: request the capability first (this shows the
  // Allow/Deny screen the first time, then is cached), then call the gated function.
  // The wallet API returns the value on success and THROWS on failure (locked /
  // rejected) — so use try/catch, not a result object.
  const ensure = (cap: string) => nema.sys.perm.request(cap) === 1;

  const showAddress = () => {
    if (!ensure("wallet.read")) { setOut("permission denied"); return; }
    try { setOut(nema.wallet.address(net, 0)); }
    catch (e) { setOut("error: " + e); }
  };

  const signMessage = () => {
    if (!ensure("wallet.sign")) { setOut("permission denied"); return; }
    try {
      const sig = nema.wallet.signMessage(net, 0, "Hello from Palanu");
      setOut("sig " + sig.slice(0, 20) + "…");
    } catch (e) { setOut("error: " + e); }
  };

  const signTransaction = () => {
    if (!ensure("wallet.sign")) { setOut("permission denied"); return; }
    try {
      const tx = nema.wallet.signTransaction(net, 0, DEMO_EVM_TX);
      setOut("tx " + tx.slice(0, 20) + "…");
    } catch (e) { setOut("error: " + e); }
  };

  return (
    <View style={{ flexDirection: "column", padding: 4, gap: 3 }}>
      <Text variant="title">Web3 Test</Text>
      <Text variant="caption">{nema.wallet.ready() ? "wallet ready" : "no wallet / locked"}</Text>

      <Pressable onPress={cycle}>
        <Text>{`Network: ${net}`}</Text>
      </Pressable>
      <Pressable onPress={showAddress}>
        <Text>Show address</Text>
      </Pressable>
      <Pressable onPress={signMessage}>
        <Text>Sign message</Text>
      </Pressable>
      <Pressable onPress={signTransaction}>
        <Text>Sign transaction (EVM)</Text>
      </Pressable>

      <ScrollView style={{ flexGrow: 1 }}>
        <Text variant="body">{out || "Pick a network, then an action."}</Text>
      </ScrollView>
    </View>
  );
}
