import React, { useState } from "react";
import { WiFiTab } from "./WiFiTab";
import { DisplayTab } from "./DisplayTab";
import { SystemTab } from "./SystemTab";

type Tab = "wifi" | "display" | "system";

export function SettingsTabs(props: {
  running: boolean;
  onBoot: () => void;
  onShutdown: () => void;
  onRestart: () => void;
  onInjectEvent: (name: string, payload: Record<string, string>) => void;
  onWifiDisconnect: () => void;
  onSend: (msg: Record<string, unknown>) => void;
  onSetResolution: (w: number, h: number) => void;
  wifi?: { connected: boolean; ssid: string; ip: string };
  resolution: { w: number; h: number };
  system: { platform: string; board: string; buildVersion: string; firmwareVersion: string } | null;
}) {
  const [tab, setTab] = useState<Tab>("wifi");

  const TABS: { id: Tab; label: string }[] = [
    { id: "wifi",    label: "WiFi" },
    { id: "display", label: "Display" },
    { id: "system",  label: "System" },
  ];

  return (
    <div style={styles.panel}>
      <div style={styles.tabBar}>
        {TABS.map(t => (
          <button key={t.id} onClick={() => setTab(t.id)}
            style={{ ...styles.tab, ...(tab === t.id ? styles.tabActive : {}) }}>
            {t.label}
          </button>
        ))}
      </div>
      <div style={styles.content}>
        {tab === "wifi" && (
          <WiFiTab onSend={props.onSend} onWifiDisconnect={props.onWifiDisconnect} wifi={props.wifi} />
        )}
        {tab === "display" && (
          <DisplayTab resolution={props.resolution} running={props.running} onSetResolution={props.onSetResolution} />
        )}
        {tab === "system" && (
          <SystemTab running={props.running} onBoot={props.onBoot} onShutdown={props.onShutdown}
                     onRestart={props.onRestart} onInjectEvent={props.onInjectEvent} system={props.system} />
        )}
      </div>
    </div>
  );
}

const styles = {
  panel:     { display: "flex", flexDirection: "column" as const, height: "100%", background: "#111", border: "1px solid #222" },
  tabBar:    { display: "flex", background: "#1a1a1a", borderBottom: "1px solid #222" },
  tab:       { flex: 1, background: "transparent", border: "none", borderBottom: "2px solid transparent", color: "#777", padding: "8px 0", fontSize: 11, fontWeight: 700, fontFamily: "inherit", cursor: "pointer", textTransform: "uppercase" as const, letterSpacing: 1 },
  tabActive: { color: "#4fc3f7", borderBottom: "2px solid #4fc3f7" },
  content:   { flex: 1, overflow: "auto", padding: "12px" },
};
