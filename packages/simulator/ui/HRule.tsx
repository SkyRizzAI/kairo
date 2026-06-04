import React from "react";
import type { StyleProps } from "./types";

interface HRuleProps extends StyleProps {
  margin?: number;
}

export function HRule({ margin = 0, style }: HRuleProps) {
  return (
    <hr style={{
      border: "none",
      borderTop: "1px solid var(--kairo-fg, #1a1a1a)",
      margin: `${margin}px 0`,
      width: "100%",
      ...style,
    }} />
  );
}
