import React from "react";
import type { StyleProps } from "./types";
import { Label } from "./Label";

interface MenuItemProps extends StyleProps {
  text: string;
  selected?: boolean;
  hint?: string;
  onClick?: () => void;
}

export function MenuItem({ text, selected = false, hint, onClick, style }: MenuItemProps) {
  return (
    <div
      onClick={onClick}
      style={{
        display: "flex",
        alignItems: "center",
        padding: "1px 4px",
        background: selected ? "var(--kairo-fg, #1a1a1a)" : "transparent",
        cursor: onClick ? "pointer" : "default",
        userSelect: "none",
        ...style,
      }}
    >
      <Label text={selected ? `> ${text}` : `  ${text}`} size="md" invert={selected} />
      {hint && !selected && (
        <span style={{ marginLeft: "auto" }}>
          <Label text={hint} size="sm" />
        </span>
      )}
    </div>
  );
}
