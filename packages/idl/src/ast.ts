// tools/idl/ast.ts — Shared AST types for the Nema System API IDL.
// Mirrors the JSON structure produced by parser.ts (api/build/nema-api.json).
// Used by all emitters and the generator orchestrator (gen.ts).

export interface TypeNode {
  kind: string;              // primitive | "option" | "list" | "result" | "tuple" | "handle" | "ref"
  inner?: TypeNode;          // option / list
  ok?: TypeNode;             // result
  err?: TypeNode;            // result
  fields?: TypeNode[];       // tuple
  name?: string;             // ref (record name)
}

export interface PidlField {
  name: string;
  type: TypeNode;
  doc?: string;
}

export interface PidlRecord {
  name: string;
  doc?: string;
  fields: PidlField[];
}

export interface PidlFunc {
  name: string;
  doc?: string;
  params: PidlField[];
  returns: TypeNode | null;
  annotations: { blocking: boolean };
}

export interface PidlInterface {
  name: string;
  doc?: string;
  annotations: { core: boolean; capability: string | null };
  functions: PidlFunc[];
}

export interface PidlPackage {
  name: string;
  major: number;
  minor: number;
  doc?: string;
  interfaces: PidlInterface[];
  records: PidlRecord[];
}

export interface PidlAst {
  version: string;
  packages: PidlPackage[];
}

export function typeToString(t: TypeNode): string {
  switch (t.kind) {
    case "handle": return "handle";
    case "ref":    return t.name!;
    case "option": return `option<${typeToString(t.inner!)}>`;
    case "list":   return `list<${typeToString(t.inner!)}>`;
    case "result": return `result<${typeToString(t.ok!)}, ${typeToString(t.err!)}>`;
    case "tuple":  return `tuple<${(t.fields || []).map(typeToString).join(", ")}>`;
    default:       return t.kind; // primitives: string, bool, u32, etc.
  }
}

export function typeToTsString(t: TypeNode): string {
  switch (t.kind) {
    case "string": return "string";
    case "bool":   return "boolean";
    case "u8":
    case "u16":
    case "u32":
    case "s32":
    case "s64":
    case "u64":
    case "f32":
    case "f64":
    case "handle": return "number";
    case "option": return `${typeToTsString(t.inner!)} | null`;
    case "list":   return `${typeToTsString(t.inner!)}[]`;
    case "ref":    return t.name!;
    case "result": return `{ ok: true; value: ${typeToTsString(t.ok!)} } | { ok: false; error: ${typeToTsString(t.err!)} }`;
    case "tuple": {
      if (!t.fields || t.fields.length === 0) return "void";
      return `[${t.fields.map(typeToTsString).join(", ")}]`;
    }
    default: return "unknown";
  }
}
