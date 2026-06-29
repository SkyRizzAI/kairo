// Web3 Test — a demo dApp for the on-device wallet (Plan 94). Built from the SAME native list
// components the firmware uses (ListContainer / ListSection / ListItemRow) so it's 1:1 with the
// built-in screens. Nested network picker (family → chain) + works on EVERY chain:
//   • Show address  — all chains (EVM share one secp256k1 address; BTC & Solana their own)
//   • Sign message  — all chains: EVM (EIP-191), Solana (Ed25519), Bitcoin (legacy signmessage)
//   • Sign tx       — EVM (RLP), Solana (SystemProgram transfer), Bitcoin (BIP143 P2WPKH)
//   • Faucet        — Solana devnet only (JSON-RPC requestAirdrop over nema.net.http.post)
//
// The signer is OFFLINE (never broadcasts; the key never leaves the device). The faucet is the
// one network call — it asks the public devnet RPC to fund the address so you can test for real.
import { ListContainer, ListSection, ListItemRow, Text, useState, useBackHandler } from "nema";

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

// Per-family demo transactions for the "Sign tx" button:
// • EVM    — an unsigned EIP-155 transfer (RLP).
// • Solana — a hand-built SystemProgram transfer message (0.01 SOL) the device clear-signs.
// • Bitcoin— a "Palanu BTC v1" P2WPKH sign-request (1-in/1-out, 0.001 → 0.0009 BTC, 0.0001 fee)
//            the device signs via BIP143. The prevout is a placeholder so it isn't broadcastable,
//            but it exercises the real sighash + witness path and shows To/Amount/Fee on-device.
const DEMO_EVM_TX =
  "e4808504a817c8008252089400000000000000000000000000000000000000008080018080";
// header(1 sig,0,1 ro) | 3 accounts | recentBlockhash | 1 instr: SystemProgram::Transfer 0.01 SOL
const DEMO_SOL_TX =
  "010001" + "03" + "11".repeat(32) + "22".repeat(32) + "00".repeat(32) + "33".repeat(32) +
  "01" + "02" + "02" + "0001" + "0c" + "02000000" + "8096980000000000";
// fmt | nVersion | nIn=1 [txid|vout|amount 100000|spk 0014<h160>|seq] | nOut=1 [amount 90000|spk] | locktime
const DEMO_BTC_TX =
  "01" + "01000000" + "01" +
  "11".repeat(32) + "00000000" + "a086010000000000" + "16" + "0014" + "22".repeat(20) + "ffffffff" +
  "01" + "905f010000000000" + "16" + "0014" + "33".repeat(20) +
  "00000000";

function demoTx(fam: string): string | null {
  if (fam === "evm") return DEMO_EVM_TX;
  if (fam === "solana") return DEMO_SOL_TX;
  if (fam === "bitcoin") return DEMO_BTC_TX;
  return null;
}

export default function App() {
  const [screen, setScreen] = useState("home");   // home | family | chains | result
  const [fam, setFam] = useState("evm");
  const [net, setNet] = useState(NETWORKS[0] || "eth-mainnet");
  const [rTitle, setRTitle] = useState("");        // result screen heading
  const [rBody, setRBody] = useState("");          // result screen full text (wrapped)

  // Physical Back: pop one level instead of exiting. On home → false = exit the app.
  // This is what makes Back behave like the built-in screens (it used to leave the app
  // from any sub-screen).
  useBackHandler(() => {
    if (screen === "result") { setScreen("home");   return true; }
    if (screen === "chains") { setScreen("family"); return true; }
    if (screen === "family") { setScreen("home");   return true; }
    return false;  // home: let the launcher take Back (exit)
  });

  const ensure = (cap: string) => nema.sys.perm.request(cap) === 1;
  // Every action ends on a dedicated result screen (full text, wrapped, Back to home)
  // instead of a cramped row at the bottom of the menu.
  const show = (title: string, body: string) => { setRTitle(title); setRBody(body); setScreen("result"); };

  const showAddress = () => {
    if (!ensure("wallet.read")) return show("Address", "Permission denied");
    try { show("Address", nema.wallet.address(net, 0)); }
    catch (e) { show("Address", "Error: " + e); }
  };
  const signMessage = () => {
    // All families: EVM (EIP-191), Solana (Ed25519 raw), Bitcoin (legacy signmessage).
    if (!ensure("wallet.sign")) return show("Sign message", "Permission denied");
    try { show("Sign message", "Signature:\n" + nema.wallet.signMessage(net, 0, "Hello from Palanu")); }
    catch (e) { show("Sign message", "Error: " + e); }
  };
  const signTx = () => {
    const tx = demoTx(familyOf(net));
    if (!tx) return show("Sign transaction", "No demo tx for this chain.");
    if (!ensure("wallet.sign")) return show("Sign transaction", "Permission denied");
    try { show("Sign transaction", "Signed tx:\n" + nema.wallet.signTransaction(net, 0, tx)); }
    catch (e) { show("Sign transaction", "Error: " + e); }
  };
  // Faucet: only Solana devnet has a programmatic airdrop (JSON-RPC requestAirdrop).
  // nema.net.http.* is a BLOCKING host call — returns synchronously, throws on a
  // transport error (no Promise/await).
  const faucet = () => {
    if (net !== "sol-devnet") return show("Faucet", "Switch to Solana Devnet first — it's the only chain with a programmatic faucet.");
    if (!ensure("net.http") || !ensure("wallet.read")) return show("Faucet", "Permission denied");
    let addr: string;
    try { addr = nema.wallet.address(net, 0); } catch (e) { return show("Faucet", "Error: " + e); }
    const body = JSON.stringify({ jsonrpc: "2.0", id: 1, method: "requestAirdrop", params: [addr, 1000000000] });
    try {
      const res = nema.net.http.post("https://api.devnet.solana.com", body, "application/json");
      const j = JSON.parse(res.body);
      if (j.result) show("Faucet", "Airdrop requested (1 SOL):\n" + j.result);
      else show("Faucet", "Faucet declined:\n" + (j.error && j.error.message ? j.error.message : "rate limited — try again later"));
    } catch (e) { show("Faucet", "Network error: " + e); }
  };

  // ── Result screen — full output, wrapped, Back returns home ──
  if (screen === "result") {
    return (
      <ListContainer>
        <ListSection title={rTitle} />
        <ListItemRow label="‹ Back" onPress={() => setScreen("home")} />
        <Text wrap style={{ padding: 4 }}>{rBody}</Text>
      </ListContainer>
    );
  }

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
      <ListItemRow label="Show address" chevron onPress={showAddress} />
      <ListItemRow label="Sign message" chevron onPress={signMessage} />
      <ListItemRow
        label="Sign transaction"
        value={demoTx(familyOf(net)) ? "" : "n/a"}
        chevron
        onPress={signTx}
      />
      <ListItemRow
        label="Faucet (airdrop)"
        value={net === "sol-devnet" ? "" : "SOL devnet"}
        chevron
        onPress={faucet}
      />
    </ListContainer>
  );
}
