// tools/idl/emit/docs.ts — Generate docs/api/*.md from the IDL AST.
// One page per interface: signature, params, annotations, type references.

import type { PidlAst, PidlInterface, PidlFunc, PidlField } from "../ast";
import { typeToString } from "../ast";

function capLabel(iface: PidlInterface): string {
  if (iface.annotations.core) return "core (always available)";
  return `gated: \`${iface.annotations.capability}\``;
}

function funcSig(fn: PidlFunc): string {
  const params = fn.params.map((p) => `${p.name}: ${typeToString(p.type)}`).join(", ");
  const ret = fn.returns ? ` → ${typeToString(fn.returns)}` : "";
  return `${fn.name}(${params})${ret}`;
}

function flags(fn: PidlFunc): string {
  const f: string[] = [];
  if (fn.annotations.blocking) f.push("`@blocking`");
  return f.length ? f.join(" ") : "—";
}

function renderInterface(pkgName: string, iface: PidlInterface): string {
  const id = `${pkgName}/${iface.name}`;
  const lines: string[] = [];

  lines.push(`# ${id}`);
  lines.push("");
  lines.push(`> ${capLabel(iface)}  `);
  lines.push(`> Package: \`${pkgName}@1.0\`  `);
  if (iface.doc) lines.push(`\n${iface.doc}\n`);

  if (iface.functions.length === 0) {
    lines.push("_(no functions)_");
    lines.push("");
    return lines.join("\n");
  }

  lines.push("## Functions");
  lines.push("");
  lines.push("| Function | Returns | Flags |");
  lines.push("|---|---|---|");

  for (const fn of iface.functions) {
    const sig = `\`${funcSig(fn)}\``;
    const ret = fn.returns ? `\`${typeToString(fn.returns)}\`` : "`void`";
    lines.push(`| ${sig} | ${ret} | ${flags(fn)} |`);
  }
  lines.push("");

  for (const fn of iface.functions) {
    lines.push(`### \`${fn.name}\``);
    lines.push("");
    if (fn.doc) lines.push(`${fn.doc}\n`);

    if (fn.params.length > 0) {
      lines.push("**Parameters:**");
      lines.push("");
      for (const p of fn.params) {
        const t = typeToString(p.type);
        lines.push(`- \`${p.name}\`: \`${t}\`${p.doc ? ` — ${p.doc}` : ""}`);
      }
      lines.push("");
    }

    if (fn.returns) {
      lines.push(`**Returns:** \`${typeToString(fn.returns)}\``);
      lines.push("");
    }
  }

  return lines.join("\n");
}

function renderIndex(ast: PidlAst): string {
  const lines: string[] = [];
  lines.push("# Nema System API Reference");
  lines.push("");
  lines.push("> Auto-generated from [`api/*.pidl`](../../../api/). Do not edit.");
  lines.push("> Version: `1.0`");
  lines.push("");

  lines.push("## Interfaces");
  lines.push("");
  lines.push("| Interface | Package | Capability | Functions |");
  lines.push("|---|---|---|---|");

  for (const pkg of ast.packages) {
    if (pkg.name.startsWith("palanu:")) continue; // TS-only wire protocol, not device API docs
    for (const iface of pkg.interfaces) {
      const id = `${pkg.name}/${iface.name}`;
      const slug = id.replace(/[:/]/g, "_");
      const cap = iface.annotations.core ? "core" : `\`${iface.annotations.capability}\``;
      const n = iface.functions.length;
      lines.push(`| [\`${id}\`](./${slug}.md) | \`${pkg.name}@${pkg.major}.${pkg.minor}\` | ${cap} | ${n} |`);
    }
  }

  lines.push("");
  lines.push("## Records");
  lines.push("");
  lines.push("| Record | Package | Fields |");
  lines.push("|---|---|---|");
  for (const pkg of ast.packages) {
    if (pkg.name.startsWith("palanu:")) continue;
    for (const rec of pkg.records) {
      const fields = rec.fields.map((f) => `\`${f.name}: ${typeToString(f.type)}\``).join(", ");
      lines.push(`| \`${rec.name}\` | \`${pkg.name}\` | ${fields} |`);
    }
  }

  return lines.join("\n");
}

export function emitDocs(ast: PidlAst): Map<string, string> {
  const files = new Map<string, string>();

  // Index
  files.set("index.md", renderIndex(ast));

  // Per-interface pages
  for (const pkg of ast.packages) {
    if (pkg.name.startsWith("palanu:")) continue;
    for (const iface of pkg.interfaces) {
      const id = `${pkg.name}/${iface.name}`;
      const slug = id.replace(/[:/]/g, "_");
      files.set(`${slug}.md`, renderInterface(pkg.name, iface));
    }
  }

  return files;
}
