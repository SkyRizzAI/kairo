import React from "react";
import type { StyleProps } from "./types";

interface ColProps extends StyleProps {
  gap?: number;
  align?: "start" | "center" | "end";
  children: React.ReactNode;
}

// Vertical layout — mirrors kairo::ui::ColLayout
export function Col({ gap = 4, align = "start", children, style }: ColProps) {
  return (
    <div style={{
      display: "flex",
      flexDirection: "column",
      alignItems: align === "start" ? "flex-start" : align === "end" ? "flex-end" : "center",
      gap: `${gap}px`,
      ...style,
    }}>
      {children}
    </div>
  );
}
