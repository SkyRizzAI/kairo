// Mirrors kairo::ui::Size enum from C++
export type Size = "xs" | "sm" | "md" | "lg" | "xl" | "2xl";

// Scale factors per size — matches scaleOf() in C++
export const SCALE: Record<Size, number> = {
  "xs":  0.6,
  "sm":  0.8,
  "md":  1,
  "lg":  2,
  "xl":  3,
  "2xl": 4,
};

// Font metrics at scale 1 (5×8 pixel font)
export const FONT_W = 5;
export const FONT_H = 8;
export const FONT_SPACING = 1;

// Derived metrics at a given scale
export function charW(scale: number) { return FONT_W * scale; }
export function charH(scale: number) { return FONT_H * scale; }
export function textW(text: string, scale: number) {
  if (!text.length) return 0;
  return text.length * (FONT_W * scale + FONT_SPACING * Math.max(scale, 1)) - FONT_SPACING * Math.max(scale, 1);
}

// Shared style props
export interface StyleProps {
  style?: React.CSSProperties;
  className?: string;
}

// Theme (eink is default — 1-bit, black on white)
export type Theme = "eink" | "phosphor" | "amber";
export const THEMES: Record<Theme, { bg: string; fg: string }> = {
  eink:     { bg: "#F0EDE0", fg: "#1a1a1a" },
  phosphor: { bg: "#0A140A", fg: "#33FF33" },
  amber:    { bg: "#0F0A00", fg: "#FFB414" },
};

import React from "react";
