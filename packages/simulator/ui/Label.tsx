import React from "react";
import type { Size, StyleProps } from "./types";
import { SCALE, FONT_H, FONT_W, FONT_SPACING } from "./types";

interface LabelProps extends StyleProps {
  text: string;
  size?: Size;
  invert?: boolean;
  color?: string;   // override fg color
}

// Retro pixel font via CSS monospace — actual pixel-perfect uses canvas; this is HTML preview.
export function Label({ text, size = "md", invert = false, color, style }: LabelProps) {
  const sc  = SCALE[size];
  const fs  = Math.max(8, Math.round((FONT_H * sc) * 0.9));   // CSS font-size approx
  const lh  = Math.round(FONT_H * sc) + 1;

  const base: React.CSSProperties = {
    fontFamily: "'JetBrains Mono','Fira Code','Courier New',monospace",
    fontSize:   fs,
    lineHeight: `${lh}px`,
    letterSpacing: sc > 1 ? `${sc - 1}px` : undefined,
    whiteSpace: "nowrap",
    display: "inline-block",
    background: invert ? "currentColor" : "transparent",
    color: invert ? "var(--kairo-bg, #F0EDE0)" : (color ?? "var(--kairo-fg, #1a1a1a)"),
    padding: invert ? "1px 2px" : 0,
    imageRendering: "pixelated",
    ...style,
  };

  return <span style={base}>{text}</span>;
}
