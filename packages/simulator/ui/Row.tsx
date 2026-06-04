import React from "react";
import type { StyleProps } from "./types";

interface RowProps extends StyleProps {
  gap?: number;
  align?: "start" | "center" | "end";
  children: React.ReactNode;
}

// Horizontal layout — mirrors kairo::ui::RowLayout
export function Row({ gap = 4, align = "center", children, style }: RowProps) {
  return (
    <div style={{
      display: "flex",
      flexDirection: "row",
      alignItems: align === "start" ? "flex-start" : align === "end" ? "flex-end" : "center",
      gap: `${gap}px`,
      ...style,
    }}>
      {children}
    </div>
  );
}
