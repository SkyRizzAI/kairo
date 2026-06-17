// tools/idl/emit/host_cpp.ts — Generate HostApi C++ abstract class (Plan 49 Fase 2).
// Emits generated/host/nema_api.gen.h with one pure virtual method per IDL function.

import type { PidlAst, PidlFunc, PidlRecord, TypeNode } from "../ast";
import { typeToString } from "../ast";

// ── IDL → C++ type mapping ────────────────────────────────────────────────

function cppType(t: TypeNode): string {
  switch (t.kind) {
    case "string": return "std::string";
    case "bool":   return "bool";
    case "u8":     return "uint8_t";
    case "u16":    return "uint16_t";
    case "u32":    return "uint32_t";
    case "u64":    return "uint64_t";
    case "s32":    return "int32_t";
    case "s64":    return "int64_t";
    case "f32":    return "float";
    case "f64":    return "double";
    case "handle": return "int32_t";
    case "option": return `std::optional<${cppType(t.inner!)}>`;
    case "list":   return `std::vector<${cppType(t.inner!)}>`;
    case "result": {
      const ok = cppType(t.ok!);
      const err = cppType(t.err!);
      return `NemaResult<${ok}, ${err}>`;
    }
    case "tuple": {
      if (!t.fields || t.fields.length === 0) return "void";
      return `std::tuple<${t.fields.map(cppType).join(", ")}>`;
    }
    case "ref": return pascalCase(t.name!);
    default: return t.kind;
  }
}

function cppParamType(t: TypeNode): string {
  // Use string_view for string params (cheap pass-by-value)
  if (t.kind === "string") return "std::string_view";
  if (t.kind === "ref" || t.kind === "list" || t.kind === "option") return `const ${cppType(t)}&`;
  return cppType(t);
}

function pascalCase(s: string): string {
  return s.split("-").map((w) => w.charAt(0).toUpperCase() + w.slice(1)).join("");
}

function methodName(ifaceName: string, funcName: string): string {
  return `${ifaceName}_${funcName}`.replace(/-/g, "_");
}

// ── Record structs ─────────────────────────────────────────────────────────

function emitRecords(records: PidlRecord[], pkg: string): string {
  if (records.length === 0) return "";
  const lines: string[] = [];
  for (const rec of records) {
    const name = pascalCase(rec.name);
    if (rec.doc) lines.push(`// ${rec.doc}`);
    lines.push(`struct ${name} {`);
    for (const f of rec.fields) {
      const fname = f.name.replace(/-/g, "_");
      lines.push(`    ${cppType(f.type)} ${fname};`);
    }
    lines.push("};");
    lines.push("");
  }
  return lines.join("\n");
}

// ── HostApi methods ────────────────────────────────────────────────────────

function emitMethod(func: PidlFunc, iface: string, domain: string): string {
  const name = methodName(iface, func.name);
  const params = func.params
    .map((p) => `${cppParamType(p.type)} ${p.name.replace(/-/g, "_")}`)
    .join(", ");
  const ret = func.returns ? cppType(func.returns) : "void";

  let doc = "";
  if (func.doc) {
    const block = func.annotations.blocking ? " [@blocking]" : "";
    doc = `    // ${domain}/${iface}.${func.name}${block}\n`;
    if (func.doc) {
      for (const line of func.doc.split("\n")) {
        doc += `    // ${line.trim()}\n`;
      }
    }
  }

  return `${doc}    virtual ${ret} ${name}(${params}) = 0;`;
}

// ── Result struct ──────────────────────────────────────────────────────────

function emitResultStruct(): string {
  return `// Generic result type for IDL result<T,E>.
template <typename T, typename E>
struct NemaResult {
    bool ok = false;
    T value{};
    E error{};
};

template <typename E>
struct NemaResult<void, E> {
    bool ok = false;
    E error{};
};
`;
}

// ── Main emitter ───────────────────────────────────────────────────────────

export function emitHostCpp(ast: PidlAst): string {
  const lines: string[] = [];

  lines.push("#pragma once");
  lines.push("");
  lines.push("#include <cstdint>");
  lines.push("#include <optional>");
  lines.push("#include <string>");
  lines.push("#include <string_view>");
  lines.push("#include <tuple>");
  lines.push("#include <vector>");
  lines.push("");

  lines.push(emitResultStruct());
  lines.push("");

  // Emit all record structs
  for (const pkg of ast.packages) {
    if (pkg.records.length > 0) {
      lines.push(`// Records from ${pkg.name}`);
      lines.push(emitRecords(pkg.records, pkg.name));
    }
  }

  lines.push("// ── HostApi — generated abstract class (Plan 49) ──────────────");
  lines.push("// Implement this ONCE in nema_host_impl.cpp (hand-written, calls rt.*).");
  lines.push("// Adding a function to the IDL without updating the impl → build error.");
  lines.push("struct HostApi {");
  lines.push("    virtual ~HostApi() = default;");
  lines.push("");

  for (const pkg of ast.packages) {
    if (pkg.interfaces.length === 0) continue;
    const domain = pkg.name.split(":")[1] || pkg.name;
    lines.push(`    // ── ${pkg.name} ───────────────────────────────────────`);
    for (const iface of pkg.interfaces) {
      for (const fn of iface.functions) {
        lines.push(emitMethod(fn, iface.name, domain));
      }
    }
    lines.push("");
  }

  lines.push("};");
  lines.push("");

  return lines.join("\n");
}
