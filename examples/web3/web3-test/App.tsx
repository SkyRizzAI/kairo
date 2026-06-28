// Web3 Test — a demo dApp for the on-device wallet (Plan 94). Built from the SAME native list
// components the firmware uses (ListContainer / ListSection / ListItemRow) so it's 1:1 with the
// built-in screens. Nested network picker (family → chain) + works on EVERY chain:
//   • Show address  — all chains (EVM share one secp256k1 address; BTC & Solana their own)
//   • Sign message  — all chains (+ trusted-display consent)
//   • Sign tx       — EVM only (the demo tx is EVM RLP; BTC/Solana use different tx formats)
//
// Note: this is an OFFLINE signer — it never broadcasts, so there's no RPC here (picking an RPC
// endpoint is the dApp's job, not the wallet's). The key never leaves the device.
import { ListContainer, ListSection, ListItemRow, useState } from "nema";

const NETWORKS: string[] = nema.wallet.networks();

function familyOf(id: string): string {
  if (id.indexOf("btc") === 0) return "bitcoin";
  if (id.indexOf("sol") === 0) return "solana";
  return "evm";
}

// Pretty names for the known ids (falls back to the raw id for anything new).
const NICE: Record<string, string> = {
  "eth-mainnet": "Ethereum", sepolia: "Sepolia", polygon: "Polygon", bnb: "BNB Chain",
  arbitrum: "Arbitrum One", optimism: "Optimism", base: "Base", avalanche: "Avalanche",
  "btc-mainnet": "Bitcoin", "btc-testnet": "Bitcoin Testnet",
  "sol-mainnet": "Solana", "sol-devnet": "Solana Devnet",
};
const labelOf = (id: string) => NICE[id] || id;

const FAMILIES = [
  { key: "evm", name: "EVM" },
  { key: "bitcoin", name: "Bitcoin" },
  { key: "solana", name: "Solana" },
];
const countIn = (fam: string) => NETWORKS.filter((n) => familyOf(n) === fam).length;
const famName = (fam: string) => (FAMILIES.find((f) => f.key === fam) || { name: fam }).name;

const DEMO_EVM_TX =
  "e4808504a817c8008252089400000000000000000000000000000000000000008080018080";

export default function App() {
  const [screen, setScreen] = useState("home");   // home | family | chains
  const [fam, setFam] = useState("evm");
  const [net, setNet] = useState(NETWORKS[0] || "eth-mainnet");
  const [out, setOut] = useState("Pick an action");

  const ensure = (cap: string) => nema.sys.perm.request(cap) === 1;

  const showAddress = () => {
    if (!ensure("wallet.read")) return setOut("Permission denied");
    try { setOut(nema.wallet.address(net, 0)); }
    catch (e) { setOut("Error: " + e); }
  };
  const signMessage = () => {
    if (!ensure("wallet.sign")) return setOut("Permission denied");
    try { setOut("sig " + nema.wallet.signMessage(net, 0, "Hello from Palanu").slice(0, 24) + "…"); }
    catch (e) { setOut("Error: " + e); }
  };
  const signTx = () => {
    if (familyOf(net) !== "evm") return setOut("Sign tx is EVM-only — use Sign message on " + famName(familyOf(net)));
    if (!ensure("wallet.sign")) return setOut("Permission denied");
    try { setOut("tx " + nema.wallet.signTransaction(net, 0, DEMO_EVM_TX).slice(0, 24) + "…"); }
    catch (e) { setOut("Error: " + e); }
  };

  // ── Sub-screen 2: chains within the chosen family ──
  if (screen === "chains") {
    return (
      <ListContainer>
        <ListSection title={famName(fam) + " networks"} />
        <ListItemRow label="‹ Back" onPress={() => setScreen("family")} />
        {NETWORKS.filter((n) => familyOf(n) === fam).map((n) => (
          <ListItemRow
            key={n}
            label={labelOf(n)}
            value={n === net ? "● active" : ""}
            onPress={() => { setNet(n); setScreen("home"); }}
          />
        ))}
      </ListContainer>
    );
  }

  // ── Sub-screen 1: network family ──
  if (screen === "family") {
    return (
      <ListContainer>
        <ListSection title="Network" />
        <ListItemRow label="‹ Back" onPress={() => setScreen("home")} />
        {FAMILIES.map((f) => (
          <ListItemRow
            key={f.key}
            label={f.name}
            value={countIn(f.key) + " chains"}
            chevron
            onPress={() => { setFam(f.key); setScreen("chains"); }}
          />
        ))}
      </ListContainer>
    );
  }

  // ── Home ──
  return (
    <ListContainer>
      <ListSection title="Web3 Test" />
      <ListItemRow label="Network" value={labelOf(net)} chevron onPress={() => setScreen("family")} />
      <ListItemRow label="Show address" onPress={showAddress} />
      <ListItemRow label="Sign message" value="all chains" onPress={signMessage} />
      <ListItemRow
        label="Sign transaction"
        value={familyOf(net) === "evm" ? "EVM" : "EVM only"}
        onPress={signTx}
      />
      <ListSection title="Result" />
      <ListItemRow label={out} />
    </ListContainer>
  );
}
