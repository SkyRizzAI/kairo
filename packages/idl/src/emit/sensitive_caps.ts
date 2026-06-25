// emit/sensitive_caps.ts — Generate the catalog of @tier(sensitive) capabilities.
//
// This is the SINGLE source of truth for user-revocable permissions. Every IDL function
// annotated `@capability(...) @tier(sensitive)` lands here automatically, so UI like
// AppDetailScreen never hand-maintains (and never drifts from) the real permission set.

import type { PidlAst } from "../ast";

// "net.wifi.monitor" → "Net Wifi Monitor". Purely derived (no per-cap hardcoding).
function prettify(cap: string): string {
  return cap.split(".").map((s) => s.charAt(0).toUpperCase() + s.slice(1)).join(" ");
}

export function emitSensitiveCaps(ast: PidlAst): string {
  const caps = new Map<string, string>(); // id → label, deduped (a cap may gate many funcs)
  for (const pkg of ast.packages) {
    for (const iface of pkg.interfaces) {
      for (const fn of iface.functions) {
        const a = fn.annotations;
        if (a.tier === "sensitive" && a.capability && !caps.has(a.capability)) {
          caps.set(a.capability, prettify(a.capability));
        }
      }
    }
  }
  const sorted = [...caps.entries()].sort((a, b) => (a[0] < b[0] ? -1 : 1));

  const lines: string[] = [];
  lines.push("#pragma once");
  lines.push("// Catalog of every @tier(sensitive) capability declared across the IDL — the single");
  lines.push("// source of truth for user-revocable permissions. Iterate kSensitiveCaps; a new");
  lines.push("// @tier(sensitive) function appears here automatically (no hand-maintained list).");
  lines.push("");
  lines.push("namespace nema {");
  lines.push("");
  lines.push("struct SensitiveCap { const char* id; const char* label; };");
  lines.push("");
  lines.push("inline constexpr SensitiveCap kSensitiveCaps[] = {");
  for (const [id, label] of sorted) lines.push(`    {"${id}", "${label}"},`);
  lines.push("};");
  lines.push(`inline constexpr int kSensitiveCapCount = ${sorted.length};`);
  lines.push("");
  lines.push("}  // namespace nema");
  lines.push("");
  return lines.join("\n");
}
