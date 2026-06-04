import React, { useState } from "react";

interface SimNet { ssid: string; password: string; rssi: number; online: boolean }

const DEFAULT_NETS: SimNet[] = [
  { ssid: "MyHomeWiFi",      password: "password123", rssi: -42, online: true },
  { ssid: "CoffeeShop_Free", password: "",            rssi: -68, online: true },
  { ssid: "Neighbour_5G",    password: "secret",      rssi: -74, online: false },
  { ssid: "AndroidAP",       password: "hotspot00",   rssi: -81, online: true },
];

export function WiFiTab({
  onSend, onWifiDisconnect, wifi,
}: {
  onSend: (msg: Record<string, unknown>) => void;
  onWifiDisconnect: () => void;
  wifi?: { connected: boolean; ssid: string; ip: string };
}) {
  const [nets, setNets] = useState<SimNet[]>(DEFAULT_NETS);

  const push = (next: SimNet[]) => {
    setNets(next);
    onSend({ cmd: "wifi_set_networks", networks: next });
  };
  const update = (i: number, patch: Partial<SimNet>) =>
    push(nets.map((n, j) => (j === i ? { ...n, ...patch } : n)));
  const remove = (i: number) => push(nets.filter((_, j) => j !== i));
  const add = () =>
    push([...nets, { ssid: "NewAP", password: "", rssi: -60, online: true }]);

  return (
    <div style={styles.body}>
      <div style={styles.label}>Nearby Networks (virtual router)</div>
      <div style={{ display: "flex", flexDirection: "column", gap: 6 }}>
        {nets.map((n, i) => (
          <div key={i} style={styles.net}>
            <div style={styles.row}>
              <input style={{ ...styles.input, flex: 1 }} value={n.ssid}
                     onChange={e => update(i, { ssid: e.target.value })} placeholder="SSID" />
              <button style={styles.x} onClick={() => remove(i)}>×</button>
            </div>
            <div style={styles.row}>
              <input style={{ ...styles.input, flex: 1 }} value={n.password}
                     onChange={e => update(i, { password: e.target.value })} placeholder="password (empty=open)" />
            </div>
            <div style={styles.row}>
              <span style={styles.hint}>RSSI</span>
              <input type="range" min={-90} max={-30} value={n.rssi}
                     onChange={e => update(i, { rssi: +e.target.value })} style={{ flex: 1 }} />
              <span style={styles.dbm}>{n.rssi}dBm</span>
              <label style={styles.toggle}>
                <input type="checkbox" checked={n.online}
                       onChange={e => update(i, { online: e.target.checked })} />
                {n.online ? "online" : "offline"}
              </label>
            </div>
          </div>
        ))}
        <Btn onClick={add} color="#4fc3f7">+ Add network</Btn>
        <div style={styles.note}>
          Device: Settings → WiFi → Scan → pick a network → type its password.
          Toggle a network <b>offline</b> to make Ticker fail even when connected.
        </div>
        {wifi && (
          <div style={styles.status}>
            {wifi.connected ? `✓ Connected: ${wifi.ssid} (${wifi.ip})` : "○ Disconnected"}
            {wifi.connected && (
              <button style={styles.smallbtn} onClick={onWifiDisconnect}>disconnect</button>
            )}
          </div>
        )}
      </div>
    </div>
  );
}

function Btn({ onClick, color, children }: { onClick: () => void; color: string; children: React.ReactNode }) {
  return (
    <button onClick={onClick}
      style={{ background: color + "22", border: `1px solid ${color}`, color, borderRadius: 4, padding: "4px 10px", cursor: "pointer", fontSize: 12, fontWeight: 700, fontFamily: "inherit" }}>
      {children}
    </button>
  );
}

const styles = {
  body:    { display: "flex", flexDirection: "column" as const, gap: 8 },
  label:   { color: "#666", fontSize: 10, fontWeight: 700, textTransform: "uppercase" as const, marginBottom: 2, letterSpacing: 1 },
  row:     { display: "flex", gap: 6, flexWrap: "wrap" as const, alignItems: "center" },
  net:     { border: "1px solid #2a2a2a", borderRadius: 5, padding: 6, display: "flex", flexDirection: "column" as const, gap: 4 },
  input:   { background: "#1a1a1a", border: "1px solid #333", color: "#ddd", borderRadius: 4, padding: "4px 8px", fontSize: 12, fontFamily: "inherit", outline: "none" },
  hint:    { color: "#777", fontSize: 11, width: 34 },
  dbm:     { color: "#999", fontSize: 10, width: 52, textAlign: "right" as const },
  toggle:  { color: "#9ccc65", fontSize: 10, display: "flex", gap: 3, alignItems: "center", cursor: "pointer" },
  x:       { background: "#2a2a2a", border: "1px solid #444", color: "#e57", borderRadius: 3, fontSize: 12, padding: "0 7px", cursor: "pointer" },
  note:    { color: "#777", fontSize: 10, lineHeight: 1.4 },
  status:  { color: "#9ccc65", fontSize: 11, marginTop: 2, display: "flex", gap: 8, alignItems: "center" },
  smallbtn:{ background: "#2a2a2a", border: "1px solid #444", color: "#aaa", borderRadius: 3, fontSize: 10, padding: "2px 6px", cursor: "pointer" },
};
