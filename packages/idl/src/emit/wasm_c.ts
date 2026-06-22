// tools/idl/emit/wasm_c.ts — Generate WASM C import header (Plan 49 / Plan 57).
//
// Emits generated/sdk/nema.h — a C89-compatible header a WASM app includes to
// call host APIs. Each IDL function becomes an `extern` declaration that wasm3
// resolves at link time from the host import table (WasmRuntime, Plan 57).
//
// Type mapping (IDL → C):
//   string  → const char*   (host-managed, valid for the call duration only)
//   bool    → uint32_t      (0=false, 1=true — WASM has no bool ABI)
//   u8/u16/u32  → uint32_t  (WASM i32)
//   u64     → uint64_t      (WASM i64)
//   s32     → int32_t
//   s64     → int64_t
//   f32     → float
//   f64     → double
//   handle  → int32_t       (opaque id)
//   result<T,E> → int32_t   (0=ok, 1=err; value returned via out-param)
//   option<T>   → value or sentinel (0 / -1 for handles)
//   list<T>  → unsupported in v1 (host allocates, returns handle)

import type { PidlAst, PidlFunc, TypeNode } from "../ast";

function cType(t: TypeNode): string {
  switch (t.kind) {
    case "string":  return "const char*";
    case "bool":    return "uint32_t";
    case "u8":
    case "u16":
    case "u32":     return "uint32_t";
    case "u64":     return "uint64_t";
    case "s32":     return "int32_t";
    case "s64":     return "int64_t";
    case "f32":     return "float";
    case "f64":     return "double";
    case "handle":  return "int32_t";
    case "option":  return cType(t.inner!);  // sentinel on host side
    case "result":  return "int32_t";        // 0=ok, 1=err
    case "list":    return "int32_t";        // opaque list handle
    case "ref":     return "int32_t";        // opaque handle for named types
    case "tuple": {
      if (!t.fields || t.fields.length === 0) return "void";
      return "int32_t";                      // composite → handle
    }
    default: return "uint32_t";
  }
}

function funcName(pkg: string, iface: string, fn: string): string {
  // e.g. "nema:sys" + "log" + "write" → "nema_sys__log_write"
  const pkgPart = pkg.replace(/[:-]/g, "_").replace(/__+/g, "_");
  return `${pkgPart}__${iface}_${fn}`.replace(/-/g, "_");
}

function emitFunc(func: PidlFunc, pkg: string, iface: string): string {
  const name = funcName(pkg, iface, func.name);
  const params: string[] = func.params.map((p) => {
    const ct = cType(p.type);
    const pname = p.name.replace(/-/g, "_");
    return `${ct} ${pname}`;
  });

  // result<T,E> → prepend out-param pointer for the ok value
  if (func.returns && func.returns.kind === "result" && func.returns.ok) {
    const okType = cType(func.returns.ok);
    if (okType !== "void") {
      params.push(`${okType}* out_value`);
    }
  } else if (func.returns && func.returns.kind !== "tuple") {
    // Non-result returns with multiple fields are handles
  }

  const ret = func.returns ? cType(func.returns) : "void";
  const paramStr = params.length ? params.join(", ") : "void";

  const lines: string[] = [];
  if (func.doc) {
    lines.push(`/* ${func.doc.split("\n")[0].trim()} */`);
  }
  // Emit capability/tier/lease requirements as a comment for app developers.
  const ann = func.annotations;
  if (ann.capability) {
    const tierStr = ann.tier ?? "ambient";
    const leaseStr = ann.lease ? " @lease" : "";
    lines.push(`/* requires: @capability("${ann.capability}") @tier(${tierStr})${leaseStr} */`);
  }
  lines.push(`extern ${ret} ${name}(${paramStr});`);
  return lines.join("\n");
}

export function emitWasmC(ast: PidlAst): string {
  const lines: string[] = [];

  lines.push("#ifndef NEMA_H");
  lines.push("#define NEMA_H");
  lines.push("");
  lines.push("#include <stdint.h>");
  lines.push("");
  lines.push("#ifdef __cplusplus");
  lines.push('extern "C" {');
  lines.push("#endif");
  lines.push("");
  lines.push("/* Nema System API — WASM import declarations (Plan 49/57).");
  lines.push(" * These are provided by the host (WasmRuntime) as WASM imports.");
  lines.push(" * Include this header in WASM app C/C++ sources.");
  lines.push(" * DO NOT call these from host (C++) code; use HostApi instead. */");
  lines.push("");

  for (const pkg of ast.packages) {
    if (pkg.name.startsWith("palanu:")) continue; // TS-only wire protocol, not WASM C API
    if (pkg.interfaces.length === 0) continue;
    lines.push(`/* ── ${pkg.name} ${"─".repeat(Math.max(0, 52 - pkg.name.length))} */`);
    for (const iface of pkg.interfaces) {
      for (const fn of iface.functions) {
        lines.push(emitFunc(fn, pkg.name, iface.name));
      }
    }
    lines.push("");
  }

  lines.push("#ifdef __cplusplus");
  lines.push("}");
  lines.push("#endif");
  lines.push("");
  lines.push("#endif /* NEMA_H */");
  lines.push("");

  return lines.join("\n");
}
