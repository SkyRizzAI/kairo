// Nema custom-app SDK public API. Apps `import { View, Text, useState } from "nema"`.
// The device provides this module to the embedded JS engine (external at build
// time) so app + host share one runtime (hooks/render identity).
export * from "./components";
export { useState, useRef, useEffect } from "./hooks";
export { renderToTree } from "./render";
// Also re-export the JSX runtime here so a SINGLE embedded bundle can serve both
// the "nema" and "nema/jsx-runtime" module names on the device engine.
export { jsx, jsxs, jsxDEV, Fragment } from "./jsx-runtime";
export type {
  Style, TextVariant, KElement, KChild, NodeDesc,
} from "./types";

// Ambient `nema` system API (http/wifi/ble/storage/timer/log) — provided by the
// device at runtime (Plan 37 Fase 4). Declared here so apps get types.
export type { NemaSystem } from "./system";
