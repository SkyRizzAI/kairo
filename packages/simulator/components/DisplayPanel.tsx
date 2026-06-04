import React, { useRef, useEffect, useState } from "react";
import type { FrameMsg } from "../lib/useSimSocket";

type Theme = "eink" | "phosphor" | "amber";

const THEMES: Record<Theme, { bg: [number,number,number]; fg: [number,number,number]; label: string }> = {
  eink:     { bg: [240, 237, 224], fg: [20,  20,  20 ], label: "E-Ink" },
  phosphor: { bg: [10,  20,  10 ], fg: [51,  255, 51 ], label: "Phosphor" },
  amber:    { bg: [15,  10,  0  ], fg: [255, 180, 20 ], label: "Amber" },
};

// Target on-screen size: scale the device up to roughly this wide.
const TARGET_PX = 528;

export function DisplayPanel({
  frame, displaySleeping, resolution,
}: {
  frame: FrameMsg | null;
  displaySleeping: boolean;
  resolution: { w: number; h: number };
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [theme, setTheme] = useState<Theme>("eink");

  const W = resolution.w;
  const H = resolution.h;
  // Integer-ish display scale so the canvas is comfortably visible.
  const SCALE = Math.max(1, Math.round(TARGET_PX / Math.max(1, W)));

  useEffect(() => {
    const ctx = canvasRef.current?.getContext("2d");
    if (!ctx) return;
    const t = THEMES[theme];

    if (!frame) {
      // Blank screen with background color
      ctx.fillStyle = `rgb(${t.bg.join(",")})`;
      ctx.fillRect(0, 0, W, H);
      return;
    }

    try {
      const raw = atob(frame.data);
      const fw = frame.width, fh = frame.height;
      const imgData = ctx.createImageData(fw, fh);
      for (let i = 0; i < fw * fh; i++) {
        const ink = raw.charCodeAt(i) > 0;
        const [r, g, b] = ink ? t.fg : t.bg;
        imgData.data[i * 4 + 0] = r;
        imgData.data[i * 4 + 1] = g;
        imgData.data[i * 4 + 2] = b;
        imgData.data[i * 4 + 3] = 255;
      }
      ctx.putImageData(imgData, 0, 0);
    } catch { /* ignore decode errors */ }
  }, [frame, theme, W, H]);

  return (
    <div style={styles.panel}>
      <div style={styles.header}>
        Display  <span style={styles.dim}>{W}×{H} · 1-bit</span>
        <span style={{ marginLeft: "auto", display: "flex", gap: 4 }}>
          {(Object.keys(THEMES) as Theme[]).map(t => (
            <button key={t} onClick={() => setTheme(t)}
              style={{ ...styles.themeBtn, opacity: theme === t ? 1 : 0.4 }}>
              {THEMES[t].label}
            </button>
          ))}
        </span>
      </div>
      <div style={styles.canvasWrap}>
        <div style={styles.bezel}>
          <canvas
            ref={canvasRef}
            width={W} height={H}
            style={{ imageRendering: "pixelated", width: W * SCALE, height: H * SCALE, display: "block" }}
          />
        </div>
        {!frame && (
          <div style={styles.noFrame}>no frame — boot the sim</div>
        )}
      </div>
    </div>
  );
}

const styles = {
  panel:       { display: "flex", flexDirection: "column" as const, height: "100%", background: "#111", border: "1px solid #222" },
  header:      { padding: "6px 10px", background: "#1a1a1a", borderBottom: "1px solid #222", fontWeight: 700, fontSize: 11, color: "#aaa", display: "flex", alignItems: "center", gap: 8 },
  dim:         { color: "#444", fontWeight: 400 },
  canvasWrap:  { flex: 1, display: "flex", alignItems: "center", justifyContent: "center", background: "#0a0a0a", overflow: "auto", position: "relative" as const, padding: 16 },
  bezel:       { padding: 10, background: "#1e1e1e", borderRadius: 10, boxShadow: "0 4px 16px rgba(0,0,0,0.6), inset 0 0 0 1px #2c2c2c" },
  noFrame:     { position: "absolute" as const, color: "#333", fontSize: 11, fontStyle: "italic" },
  themeBtn:    { background: "transparent", border: "1px solid #444", color: "#aaa", borderRadius: 3, padding: "2px 6px", cursor: "pointer", fontSize: 10, fontFamily: "inherit" },
};
