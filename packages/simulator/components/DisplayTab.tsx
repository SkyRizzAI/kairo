import React, { useState } from "react";

interface Preset { label: string; w: number; h: number }

// Common IoT/embedded display panels. The dev-board panel is marked so it's
// obvious which one the real hardware uses.
const PRESETS: Preset[] = [
  // E-ink / e-paper
  { label: "GDEY027T91 2.7\" (dev-board)", w: 264, h: 176 },
  { label: "1.54\" E-ink (200×200)",       w: 200, h: 200 },
  { label: "2.13\" E-ink (250×122)",       w: 250, h: 122 },
  { label: "2.9\" E-ink (296×128)",        w: 296, h: 128 },
  { label: "4.2\" E-ink (400×300)",        w: 400, h: 300 },
  { label: "5.83\" E-ink (648×480)",       w: 648, h: 480 },
  { label: "7.5\" E-ink (800×480)",        w: 800, h: 480 },
  // OLED (monochrome)
  { label: "0.91\" OLED (128×32)",         w: 128, h: 32  },
  { label: "0.96\" OLED (128×64)",         w: 128, h: 64  },
  { label: "1.3\" OLED (128×64)",          w: 128, h: 64  },
  { label: "2.42\" OLED (128×64)",         w: 128, h: 64  },
  // LCD (monochrome / Nokia-style)
  { label: "Nokia 5110 LCD (84×48)",       w: 84,  h: 48  },
  { label: "ST7920 LCD (128×64)",          w: 128, h: 64  },
  // Larger TFT-class panels (rendered 1-bit here)
  { label: "Flipper-style (128×64)",       w: 128, h: 64  },
  { label: "320×240 (QVGA)",               w: 320, h: 240 },
  { label: "480×320 (HVGA)",               w: 480, h: 320 },
];

export function DisplayTab({
  resolution, running, onSetResolution,
}: {
  resolution: { w: number; h: number };
  running: boolean;
  onSetResolution: (w: number, h: number) => void;
}) {
  // Detect if current resolution matches a preset; else "Custom"
  const matchIdx = PRESETS.findIndex(p => p.w === resolution.w && p.h === resolution.h);
  const [sel, setSel] = useState<number>(matchIdx >= 0 ? matchIdx : -1);
  const [customW, setCustomW] = useState<number>(resolution.w);
  const [customH, setCustomH] = useState<number>(resolution.h);

  const isCustom = sel === -1;
  const pendingW = isCustom ? customW : PRESETS[sel].w;
  const pendingH = isCustom ? customH : PRESETS[sel].h;
  const changed  = pendingW !== resolution.w || pendingH !== resolution.h;

  return (
    <div style={styles.body}>
      <div style={styles.label}>Display Panel</div>

      <select
        value={sel}
        onChange={e => setSel(Number(e.target.value))}
        style={styles.select}>
        {PRESETS.map((p, i) => (
          <option key={i} value={i}>{p.label}</option>
        ))}
        <option value={-1}>Custom…</option>
      </select>

      {isCustom && (
        <div style={styles.row}>
          <span style={styles.hint}>W</span>
          <input type="number" min={64} max={1024} value={customW}
                 onChange={e => setCustomW(Number(e.target.value))} style={styles.num} />
          <span style={styles.hint}>H</span>
          <input type="number" min={32} max={768} value={customH}
                 onChange={e => setCustomH(Number(e.target.value))} style={styles.num} />
        </div>
      )}

      <div style={styles.current}>
        Active: <b>{resolution.w}×{resolution.h}</b>
        {changed && <span style={styles.pending}> → {pendingW}×{pendingH}</span>}
      </div>

      <button
        disabled={!changed}
        onClick={() => onSetResolution(pendingW, pendingH)}
        style={{ ...styles.apply, opacity: changed ? 1 : 0.4, cursor: changed ? "pointer" : "default" }}>
        Apply &amp; {running ? "Restart" : "Boot"}
      </button>

      <div style={styles.note}>
        Applying changes the virtual panel size and {running ? "restarts" : "boots"} the
        simulator. UI is resolution-independent — layout adapts to the new size.
      </div>
    </div>
  );
}

const styles = {
  body:    { display: "flex", flexDirection: "column" as const, gap: 10 },
  label:   { color: "#666", fontSize: 10, fontWeight: 700, textTransform: "uppercase" as const, letterSpacing: 1 },
  select:  { background: "#1a1a1a", border: "1px solid #333", color: "#ddd", borderRadius: 4, padding: "6px 8px", fontSize: 12, fontFamily: "inherit", outline: "none" },
  row:     { display: "flex", gap: 6, alignItems: "center" },
  hint:    { color: "#777", fontSize: 11 },
  num:     { background: "#1a1a1a", border: "1px solid #333", color: "#ddd", borderRadius: 4, padding: "4px 8px", fontSize: 12, fontFamily: "inherit", outline: "none", width: 70 },
  current: { color: "#999", fontSize: 12 },
  pending: { color: "#4fc3f7" },
  apply:   { background: "#4fc3f722", border: "1px solid #4fc3f7", color: "#4fc3f7", borderRadius: 4, padding: "6px 10px", fontSize: 12, fontWeight: 700, fontFamily: "inherit" },
  note:    { color: "#777", fontSize: 10, lineHeight: 1.4 },
};
