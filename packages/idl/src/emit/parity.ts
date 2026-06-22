// tools/idl/emit/parity.ts — Generate docs/api/parity.md coverage matrix.
// Tracks: host C++, QuickJS binding, WASM C header, JS .d.ts, docs, host impl.

import type { PidlAst } from "../ast";

export function emitParity(ast: PidlAst): string {
  const lines: string[] = [];

  lines.push("# Nema System API — Parity Matrix");
  lines.push("");
  lines.push("> Auto-generated from `api/build/nema-api.json`. Do not edit.");
  lines.push("> Host API version: `1.0`");
  lines.push("");

  // Collect all interface::function pairs
  interface Row {
    iface: string;
    fn: string;
    blocking: boolean;
    capability: string | null;
    tier: string | null;
    lease: boolean;
  }
  const rows: Row[] = [];
  for (const pkg of ast.packages) {
    if (pkg.name.startsWith("palanu:")) continue; // TS-only wire protocol, not device API parity
    for (const iface of pkg.interfaces) {
      for (const fn of iface.functions) {
        rows.push({
          iface: `${pkg.name}/${iface.name}`,
          fn: fn.name,
          blocking: fn.annotations.blocking,
          capability: fn.annotations.capability ?? iface.annotations.capability ?? null,
          tier: fn.annotations.tier ?? null,
          lease: fn.annotations.lease,
        });
      }
    }
  }

  lines.push("## Coverage");
  lines.push("");
  lines.push(`**Total functions:** ${rows.length}`);
  lines.push("");
  lines.push("| Interface | Function | Cap | Tier | Host C++ | QuickJS | WASM .h | JS .d.ts | Docs | Impl |");
  lines.push("|---|---|---|---|---|---|---|---|---|---|");

  for (const row of rows) {
    const hostCpp = "—";
    const quickjs = "—";
    const wasmH = "—";
    const dts = "—";
    const docs = "✅";
    const impl = "—";

    const flags = [
      row.blocking ? "🔒" : "",
      row.lease    ? "🔑" : "",
    ].filter(Boolean).join(" ");
    const fnCell = flags ? `\`${row.fn}\` ${flags}` : `\`${row.fn}\``;
    const capCell = row.capability ? `\`${row.capability}\`` : "—";
    const tierCell = row.tier ?? "—";
    lines.push(`| \`${row.iface}\` | ${fnCell} | ${capCell} | ${tierCell} | ${hostCpp} | ${quickjs} | ${wasmH} | ${dts} | ${docs} | ${impl} |`);
  }

  lines.push("");

  lines.push("## Legend");
  lines.push("");
  lines.push("| Symbol | Meaning |");
  lines.push("|---|---|");
  lines.push("| ✅ | Generated / implemented |");
  lines.push("| — | Not yet generated (pending phase) |");
  lines.push("| 🔒 | `@blocking` — must run on TaskRunner worker |");
  lines.push("| 🔑 | `@lease` — requires ResourceBroker exclusive grant |");

  return lines.join("\n");
}
