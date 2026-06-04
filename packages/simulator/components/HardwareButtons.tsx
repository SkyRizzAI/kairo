import React from "react";

// Physical-device button layout: D-pad (4 arrows) on the left, SELECT + CANCEL
// on the right — mirrors the real hardware (4 arrows + 2 action keys).
export function HardwareButtons({
  onKey, running,
}: {
  onKey: (k: string) => void;
  running: boolean;
}) {
  const dis = !running;
  return (
    <div style={styles.wrap}>
      {/* D-pad: 3×3 grid, arrows on the cross, empty corners */}
      <div style={styles.dpad}>
        <span />
        <PadBtn k="Up"    label="▲" dis={dis} onKey={onKey} />
        <span />
        <PadBtn k="Left"  label="◄" dis={dis} onKey={onKey} />
        <span style={styles.dpadCenter} />
        <PadBtn k="Right" label="►" dis={dis} onKey={onKey} />
        <span />
        <PadBtn k="Down"  label="▼" dis={dis} onKey={onKey} />
        <span />
      </div>

      {/* Action buttons: SELECT (green), CANCEL (red), stacked vertically */}
      <div style={styles.actions}>
        <ActBtn k="Select" label="●" sub="SELECT" color="#4caf50" dis={dis} onKey={onKey} />
        <ActBtn k="Cancel" label="✕" sub="CANCEL" color="#ef5350" dis={dis} onKey={onKey} />
      </div>
    </div>
  );
}

function PadBtn({ k, label, dis, onKey }: { k: string; label: string; dis: boolean; onKey: (k: string) => void }) {
  return (
    <button disabled={dis} onClick={() => onKey(k)}
      style={{ ...styles.padBtn, color: dis ? "#333" : "#ccc", cursor: dis ? "default" : "pointer" }}
      onMouseDown={e => !dis && (e.currentTarget.style.transform = "translateY(1px)")}
      onMouseUp={e => (e.currentTarget.style.transform = "")}
      onMouseLeave={e => (e.currentTarget.style.transform = "")}>
      {label}
    </button>
  );
}

function ActBtn({ k, label, sub, color, dis, onKey }: {
  k: string; label: string; sub: string; color: string; dis: boolean; onKey: (k: string) => void;
}) {
  return (
    <button disabled={dis} onClick={() => onKey(k)}
      style={{
        ...styles.actBtn,
        borderColor: dis ? "#333" : color,
        color: dis ? "#333" : color,
        cursor: dis ? "default" : "pointer",
      }}
      onMouseDown={e => !dis && (e.currentTarget.style.transform = "translateY(1px)")}
      onMouseUp={e => (e.currentTarget.style.transform = "")}
      onMouseLeave={e => (e.currentTarget.style.transform = "")}>
      <span style={{ fontSize: 18, lineHeight: 1 }}>{label}</span>
      <span style={{ fontSize: 8, letterSpacing: 1, marginTop: 2 }}>{sub}</span>
    </button>
  );
}

const RAISED = "0 2px 0 #000, inset 0 0 0 1px #3a3a3a";

const styles = {
  wrap:        { display: "flex", gap: 28, alignItems: "center", justifyContent: "center", padding: "14px 0" },
  dpad:        { display: "grid", gridTemplateColumns: "repeat(3, 40px)", gridTemplateRows: "repeat(3, 40px)", gap: 4 },
  dpadCenter:  { background: "#181818", borderRadius: 4 },
  padBtn:      { background: "#262626", border: "none", borderRadius: 6, fontSize: 16, fontWeight: 700, fontFamily: "inherit", boxShadow: RAISED, display: "flex", alignItems: "center", justifyContent: "center", transition: "transform 0.05s" },
  actions:     { display: "flex", flexDirection: "column" as const, gap: 12 },
  actBtn:      { width: 64, height: 56, background: "#262626", border: "2px solid", borderRadius: 10, fontWeight: 700, fontFamily: "inherit", boxShadow: RAISED, display: "flex", flexDirection: "column" as const, alignItems: "center", justifyContent: "center", transition: "transform 0.05s" },
};
