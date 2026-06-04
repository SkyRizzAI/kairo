import React from "react";
import type { Size, StyleProps } from "./types";
import { SCALE, FONT_H } from "./types";
import { Label } from "./Label";

interface ButtonProps extends StyleProps {
  text: string;
  size?: Size;
  selected?: boolean;
  onClick?: () => void;
  disabled?: boolean;
  paddingX?: number;
  paddingY?: number;
}

export function Button({
  text, size = "md", selected = false, onClick, disabled = false,
  paddingX = 4, paddingY = 2, style
}: ButtonProps) {
  const sc  = SCALE[size];
  const px  = paddingX * sc;
  const py  = paddingY * sc;
  const bw  = Math.round(1 * Math.max(sc, 1));  // border width scales with size

  const base: React.CSSProperties = {
    display: "inline-flex",
    alignItems: "center",
    justifyContent: "center",
    padding: `${py}px ${px}px`,
    border: `${bw}px solid var(--kairo-fg, #1a1a1a)`,
    background: selected ? "var(--kairo-fg, #1a1a1a)" : "transparent",
    cursor: disabled ? "default" : "pointer",
    opacity: disabled ? 0.4 : 1,
    userSelect: "none",
    ...style,
  };

  return (
    <button
      style={base}
      onClick={disabled ? undefined : onClick}
      disabled={disabled}
    >
      <Label text={text} size={size} invert={selected} />
    </button>
  );
}
