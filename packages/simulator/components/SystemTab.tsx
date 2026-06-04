import React, { useState } from "react";

export function SystemTab({
  running, onBoot, onShutdown, onRestart, onInjectEvent, system,
}: {
  running: boolean;
  onBoot: () => void;
  onShutdown: () => void;
  onRestart: () => void;
  onInjectEvent: (name: string, payload: Record<string, string>) => void;
  system: { platform: string; board: string; buildVersion: string; firmwareVersion: string } | null;
}) {
  const [evtName, setEvtName] = useState("CustomEvent");

  return (
    <div style={styles.body}>
      <div style={styles.label}>Runtime</div>
      <div style={styles.row}>
        <Btn disabled={running}  onClick={onBoot}     color="#4caf50">Boot</Btn>
        <Btn disabled={!running} onClick={onShutdown} color="#ef5350">Shutdown</Btn>
        <Btn disabled={!running} onClick={onRestart}  color="#ff9800">Restart</Btn>
      </div>

      <div style={styles.label}>Inject Event</div>
      <div style={styles.row}>
        <input style={{ ...styles.input, flex: 1 }} value={evtName}
               onChange={e => setEvtName(e.target.value)} placeholder="Event name" />
        <Btn disabled={!running || !evtName} onClick={() => onInjectEvent(evtName, {})} color="#ba68c8">
          Inject
        </Btn>
      </div>

      {system && (
        <>
          <div style={styles.label}>Device</div>
          <div style={styles.info}>
            <div>Platform: <b>{system.platform}</b></div>
            <div>Board: <b>{system.board}</b></div>
            <div>Build: <b>{system.buildVersion}</b></div>
            <div>Firmware: <b>{system.firmwareVersion}</b></div>
          </div>
        </>
      )}
    </div>
  );
}

function Btn({ disabled, onClick, color, children }: { disabled: boolean; onClick: () => void; color: string; children: React.ReactNode }) {
  return (
    <button disabled={disabled} onClick={onClick}
      style={{ background: disabled ? "#2a2a2a" : color + "22", border: `1px solid ${disabled ? "#333" : color}`, color: disabled ? "#444" : color, borderRadius: 4, padding: "4px 10px", cursor: disabled ? "default" : "pointer", fontSize: 12, fontWeight: 700, fontFamily: "inherit" }}>
      {children}
    </button>
  );
}

const styles = {
  body:  { display: "flex", flexDirection: "column" as const, gap: 8 },
  label: { color: "#666", fontSize: 10, fontWeight: 700, textTransform: "uppercase" as const, letterSpacing: 1, marginTop: 4 },
  row:   { display: "flex", gap: 6, alignItems: "center" },
  input: { background: "#1a1a1a", border: "1px solid #333", color: "#ddd", borderRadius: 4, padding: "4px 8px", fontSize: 12, fontFamily: "inherit", outline: "none" },
  info:  { color: "#999", fontSize: 11, lineHeight: 1.6 },
};
