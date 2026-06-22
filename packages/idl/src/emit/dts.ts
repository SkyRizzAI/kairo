// tools/idl/emit/dts.ts — Generate nema.d.ts TypeScript declarations (Plan 49 Fase 3).
// Replaces the hand-written system.ts with IDL-derived types.

import type { PidlAst, PidlFunc, PidlInterface } from "../ast";
import { typeToTsString } from "../ast";

function camelCase(s: string): string { const p = s.split("-").map(w => w.charAt(0).toUpperCase() + w.slice(1)).join(""); return p.charAt(0).toLowerCase() + p.slice(1); }

function funcToTs(fn: PidlFunc, indent: string): string {
  const name = camelCase(fn.name);
  const params = fn.params.map((p) => `${camelCase(p.name)}: ${typeToTsString(p.type)}`).join(", ");
  const ret = fn.returns ? typeToTsString(fn.returns) : "void";
  if (fn.annotations.blocking) {
    return `${indent}${name}(${params}): Promise<${ret}>;`;
  }
  return `${indent}${name}(${params}): ${ret};`;
}

export function emitDts(ast: PidlAst): string {
  const lines: string[] = [];
  lines.push("declare namespace nema {");

  // Group interfaces by domain
  const domains = new Map<string, PidlInterface[]>();
  for (const pkg of ast.packages) {
    if (pkg.name.startsWith("palanu:")) continue; // TS-only wire protocol, not device JS API
    const domain = pkg.name.split(":")[1] || pkg.name;
    if (!domains.has(domain)) domains.set(domain, []);
    for (const iface of pkg.interfaces) {
      domains.get(domain)!.push(iface);
    }
  }

  // Emit namespaces
  for (const [domain, ifaces] of domains) {
    lines.push(`  namespace ${domain} {`);

    for (const iface of ifaces) {
      const capNote = iface.annotations.capability
        ? `  // gated: ${iface.annotations.capability}`
        : "  // core";
      lines.push(`    ${capNote}`);
      lines.push(`    namespace ${iface.name} {`);

      for (const fn of iface.functions) {
        if (fn.doc) {
          for (const dline of fn.doc.split("\n")) {
            lines.push(`      /** ${dline.trim()} */`);
          }
        }
        lines.push(`      ${funcToTs(fn, "      ")}`);
      }

      lines.push("    }");
      lines.push("");
    }

    lines.push("  }");
    lines.push("");
  }

  lines.push("  // ── Deprecated aliases (Plan 48 Fase 4) ──");
  lines.push("  /** @deprecated Use nema.sys.log.log() */");
  lines.push("  function log(level: string, tag: string, msg: string): void;");
  lines.push("  /** @deprecated Use nema.sys.device */");
  lines.push("  const device: typeof nema.sys.device;");
  lines.push("  /** @deprecated Use nema.storage */");
  lines.push("  namespace storage {");
  lines.push("    /** @deprecated Use nema.storage.kv.get */");
  lines.push("    function get(key: string): string | null;");
  lines.push("    /** @deprecated Use nema.storage.kv.set */");
  lines.push("    function set(key: string, value: string): void;");
  lines.push("    /** @deprecated Use nema.storage.kv.remove */");
  lines.push("    function remove(key: string): boolean;");
  lines.push("  }");
  lines.push("  /** @deprecated Use nema.net.http */");
  lines.push("  namespace http {");
  lines.push("    function get(url: string): Promise<{ status: number; body: string }>;");
  lines.push("  }");

  lines.push("}");
  lines.push("");

  // Result types
  lines.push("declare type NemaResult<T, E> = { ok: true; value: T } | { ok: false; error: E };");
  lines.push("");

  return lines.join("\n");
}
