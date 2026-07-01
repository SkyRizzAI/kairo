#!/usr/bin/env bun
// packages/idl/src/parser.ts — WIT-subset .pidl → JSON AST parser (Plan 48 Fase 2).
//
// Reads api/*.pidl, produces api/build/nema-api.json.
// Subset: interface / func / record / enum / result / option / list / handle.
// No world / generics / resource-rich. Target: ~300 LOC.
//
// Usage:
//   bun run packages/idl/src/parser.ts              # parse & write api/build/nema-api.json
//   bun run packages/idl/src/parser.ts --check      # validate only (no write)

import { readdir, readFile, writeFile } from "node:fs/promises";
import { join } from "node:path";

// ── Types ──────────────────────────────────────────────────────────────────

interface TypeNode {
  kind: string;              // primitive | "option" | "list" | "result" | "tuple" | "handle" | "ref"
  inner?: TypeNode;          // option / list
  ok?: TypeNode;             // result
  err?: TypeNode;            // result
  fields?: TypeNode[];       // tuple
  name?: string;             // ref (record name)
}

interface PidlField {
  name: string;
  type: TypeNode;
  doc?: string;
}

interface PidlRecord {
  name: string;
  doc?: string;
  fields: PidlField[];
}

interface PidlFunc {
  name: string;
  doc?: string;
  params: PidlField[];
  returns: TypeNode | null;
  annotations: FuncAnnotations;
}

interface FuncAnnotations {
  blocking: boolean;
  capability: string | null;
  tier: 'ambient' | 'benign' | 'sensitive' | null;
  lease: boolean;
}

interface PidlInterface {
  name: string;
  doc?: string;
  annotations: InterfaceAnnotations;
  functions: PidlFunc[];
}

interface InterfaceAnnotations {
  core: boolean;
  capability: string | null;   // capability name string, e.g. "net.http"
}

interface PidlPackage {
  name: string;
  major: number;
  minor: number;
  doc?: string;
  interfaces: PidlInterface[];
  records: PidlRecord[];
}

interface PidlAst {
  version: string;             // "1.0"
  packages: PidlPackage[];
}

// ── Capability catalog (Plan 42) ───────────────────────────────────────────

const KNOWN_CAPS = new Set([
  "display", "input", "input.prev", "input.next", "input.activate",
  "input.back", "input.adjust", "input.2d", "input.touch",
  "camera", "audio.input", "audio.output", "rgb",
  "led", "led.rgb",
  "sensors", "sensors.environment", "sensors.light", "sensors.motion",
  "battery",
  "net.wifi", "net.wifi.managed", "net.wifi.scan",
  "net.wifi.monitor", "net.wifi.inject", "net.wifi.ap",
  "net.http", "bt.ble",
  "storage", "remote.usb", "profile",
  "wallet.read", "wallet.sign",
]);

const PRIMITIVES = new Set([
  "string", "bool",
  "u8", "u16", "u32", "u64",
  "s32", "s64",
  "f32", "f64",
]);

// ── Type parser ────────────────────────────────────────────────────────────

function splitTypeArgs(str: string): string[] {
  const result: string[] = [];
  let depth = 0, current = "";
  for (const ch of str) {
    if (ch === "<") depth++;
    else if (ch === ">") depth--;
    else if (ch === "," && depth === 0) {
      result.push(current.trim());
      current = "";
      continue;
    }
    current += ch;
  }
  if (current.trim()) result.push(current.trim());
  return result;
}

function parseType(raw: string): TypeNode {
  const str = raw.trim();
  if (!str) throw new Error(`empty type string`);

  if (str === "handle") return { kind: "handle" };
  if (PRIMITIVES.has(str)) return { kind: str };

  const mOpt = str.match(/^option<(.+)>$/);
  if (mOpt) return { kind: "option", inner: parseType(mOpt[1]) };

  const mList = str.match(/^list<(.+)>$/);
  if (mList) return { kind: "list", inner: parseType(mList[1]) };

  const mResult = str.match(/^result<(.+),\s*(.+)>$/);
  if (mResult) return { kind: "result", ok: parseType(mResult[1]), err: parseType(mResult[2]) };

  const mTuple = str.match(/^tuple<(.*)>$/);
  if (mTuple) {
    const inner = mTuple[1].trim();
    if (!inner) return { kind: "tuple", fields: [] };
    return { kind: "tuple", fields: splitTypeArgs(inner).map(parseType) };
  }

  // Named record reference
  return { kind: "ref", name: str };
}

// ── Line classification ────────────────────────────────────────────────────

type LineKind =
  | "blank"
  | "comment"       // //
  | "doc"           // ///
  | "annotation"    // @xxx or @xxx("yyy")
  | "package"
  | "record"
  | "interface"
  | "function"
  | "field"
  | "close";        // }

interface ParsedLine {
  kind: LineKind;
  text: string;       // stripped line
  param?: string;     // annotation param ("net.http")
}

function classify(line: string): ParsedLine {
  const trimmed = line.trim();
  if (!trimmed) return { kind: "blank", text: "" };

  if (trimmed.startsWith("///")) return { kind: "doc", text: trimmed.slice(3).trim() };
  if (trimmed.startsWith("//")) return { kind: "comment", text: trimmed.slice(2).trim() };
  if (trimmed === "}") return { kind: "close", text: "" };

  // Handles: @name  @name("quoted")  @name(unquoted)
  const annMatch = trimmed.match(/^@(\w[\w-]*)(?:\("?([^")\s]*)"?\))?$/);
  if (annMatch) {
    return { kind: "annotation", text: annMatch[1], param: annMatch[2] || undefined };
  }

  const pkgMatch = trimmed.match(/^package\s+(\S+)@(\d+)\.(\d+)$/);
  if (pkgMatch) return { kind: "package", text: pkgMatch[1], param: `${pkgMatch[2]}.${pkgMatch[3]}` };

  const recMatch = trimmed.match(/^record\s+(\S[\w-]*)\s*\{$/);
  if (recMatch) return { kind: "record", text: recMatch[1] };

  const ifaceMatch = trimmed.match(/^interface\s+(\S[\w-]*)\s*\{$/);
  if (ifaceMatch) return { kind: "interface", text: ifaceMatch[1] };

  // func: name: func(params) -> ret  OR  name: func(params)
  const funcMatch = trimmed.match(/^([\w-]+)\s*:\s*func\((.*)\)\s*(?:->\s*(.+))?$/);
  if (funcMatch) return {
    kind: "function",
    text: funcMatch[1],
    param: JSON.stringify({ params: funcMatch[2], ret: funcMatch[3]?.trim() || null }),
  };

  // field: name: type,
  const fieldMatch = trimmed.match(/^([\w-]+)\s*:\s*(.+),$/);
  if (fieldMatch) return { kind: "field", text: fieldMatch[1], param: fieldMatch[2] };

  throw new Error(`Cannot parse line: "${trimmed}"`);
}

// ── Collect annotations from accumulated @-lines ───────────────────────────

function collectAnnotations(annLines: ParsedLine[]): InterfaceAnnotations {
  const result: InterfaceAnnotations = { core: false, capability: null };
  for (const l of annLines) {
    if (l.text === "core") result.core = true;
    else if (l.text === "capability") result.capability = l.param || null;
    // ignore unknown annotations (forward-compatible)
  }
  return result;
}

function collectFuncAnnotations(annLines: ParsedLine[]): FuncAnnotations {
  const result: FuncAnnotations = { blocking: false, capability: null, tier: null, lease: false };
  for (const l of annLines) {
    if (l.text === "blocking") result.blocking = true;
    else if (l.text === "capability") result.capability = l.param || null;
    else if (l.text === "tier") {
      const t = l.param;
      if (t === "ambient" || t === "benign" || t === "sensitive") result.tier = t;
    }
    else if (l.text === "lease") result.lease = true;
    // unknown annotations silently ignored (forward-compatible)
  }
  return result;
}

// ── Parse one .pidl file ───────────────────────────────────────────────────

interface ParseContext {
  file: string;
  lineNo: number;
}

function err(ctx: ParseContext, msg: string): never {
  throw new Error(`${ctx.file}:${ctx.lineNo}: ${msg}`);
}

function parseOneFile(source: string, ctx: ParseContext): PidlPackage {
  const lines = source.split("\n").map((l) => l.trimEnd()); // preserve trailing ,
  let pkg: PidlPackage | null = null;
  let pkgDoc = "";

  let currentRecord: PidlRecord | null = null;
  let currentInterface: PidlInterface | null = null;

  let pendingAnnotations: ParsedLine[] = [];
  let pendingFuncAnnotations: ParsedLine[] = [];
  let pendingDoc = "";

  function flushDoc(): string {
    const d = pendingDoc;
    pendingDoc = "";
    return d;
  }

  function flushAnnotations(): InterfaceAnnotations {
    const a = collectAnnotations(pendingAnnotations);
    pendingAnnotations = [];
    return a;
  }

  function flushFuncAnnotations(): FuncAnnotations {
    const a = collectFuncAnnotations(pendingFuncAnnotations);
    pendingFuncAnnotations = [];
    return a;
  }

  for (let i = 0; i < lines.length; i++) {
    ctx.lineNo = i + 1;
    const raw = lines[i];
    const pl = classify(raw);

    switch (pl.kind) {
      case "blank":
        break;

      case "comment":
        break;

      case "doc":
        // Doc comments accumulate; they attach to the NEXT declaration.
        pendingDoc = pendingDoc ? pendingDoc + " " + pl.text : pl.text;
        break;

      case "annotation":
        // Annotations apply to the NEXT interface or function.
        // We don't know which yet, so accumulate in both queues; the consumer
        // decides which queue to drain based on context.
        pendingAnnotations.push(pl);
        pendingFuncAnnotations.push(pl);
        break;

      case "package": {
        if (pkg) err(ctx, "duplicate package declaration");
        const [major, minor] = pl.param!.split(".").map(Number);
        pkg = {
          name: pl.text,
          major,
          minor,
          doc: flushDoc(),
          interfaces: [],
          records: [],
        };
        flushAnnotations(); // discard any stray annotations before package
        flushFuncAnnotations();
        break;
      }

      case "record": {
        if (!pkg) err(ctx, "record before package declaration");
        if (currentInterface) err(ctx, `record inside interface "${currentInterface.name}"`);
        currentRecord = {
          name: pl.text,
          doc: flushDoc(),
          fields: [],
        };
        flushAnnotations(); // records don't have annotations in our subset
        flushFuncAnnotations();
        break;
      }

      case "interface": {
        if (!pkg) err(ctx, "interface before package declaration");
        if (currentRecord) err(ctx, `interface inside record "${currentRecord.name}"`);
        const ann = flushAnnotations();
        const funcAnn = flushFuncAnnotations(); // stray @blocking on interface → ignore
        currentInterface = {
          name: pl.text,
          doc: flushDoc(),
          annotations: ann,
          functions: [],
        };
        break;
      }

      case "function": {
        if (!currentInterface) err(ctx, "function outside interface");
        const ann = flushFuncAnnotations();
        const { params: paramStr, ret } = JSON.parse(pl.param!);
        const params: PidlField[] = paramStr
          ? splitTypeArgs(paramStr).map((p: string) => {
              const m = p.trim().match(/^([\w-]+)\s*:\s*(.+)$/);
              if (!m) throw err(ctx, `bad param: "${p}"`);
              return { name: m[1], type: parseType(m[2]) };
            })
          : [];
        currentInterface.functions.push({
          name: pl.text,
          doc: flushDoc(),
          params,
          returns: ret ? parseType(ret) : null,
          annotations: ann,
        });
        break;
      }

      case "field": {
        if (!currentRecord) err(ctx, "field outside record");
        currentRecord.fields.push({
          name: pl.text,
          type: parseType(pl.param!),
          doc: flushDoc(),
        });
        break;
      }

      case "close": {
        if (currentRecord) {
          pkg!.records.push(currentRecord);
          currentRecord = null;
        } else if (currentInterface) {
          pkg!.interfaces.push(currentInterface);
          currentInterface = null;
        } else {
          err(ctx, "unmatched }");
        }
        break;
      }
    }
  }

  if (!pkg) throw err(ctx, "missing package declaration");
  return pkg;
}

// ── Validation ─────────────────────────────────────────────────────────────

function validate(ast: PidlAst) {
  const allRecords = new Map<string, PidlRecord>();
  const allInterfaces = new Set<string>();

  for (const pkg of ast.packages) {
    // Check for duplicate interfaces within a package
    const ifaceNames = new Set<string>();
    for (const iface of pkg.interfaces) {
      const full = `${pkg.name}/${iface.name}`;
      if (allInterfaces.has(full))
        throw new Error(`Duplicate interface: ${full}`);
      allInterfaces.add(full);
      if (ifaceNames.has(iface.name))
        throw new Error(`Duplicate interface "${iface.name}" in package ${pkg.name}`);
      ifaceNames.add(iface.name);
    }

    // Check for duplicate records within a package
    const recNames = new Set<string>();
    for (const rec of pkg.records) {
      if (recNames.has(rec.name))
        throw new Error(`Duplicate record "${rec.name}" in package ${pkg.name}`);
      recNames.add(rec.name);
      allRecords.set(rec.name, rec);
    }

    // Validate capability references (interface-level and function-level)
    for (const iface of pkg.interfaces) {
      if (iface.annotations.capability) {
        if (!KNOWN_CAPS.has(iface.annotations.capability))
          throw new Error(
            `Unknown capability "${iface.annotations.capability}" in ${pkg.name}/${iface.name}`
          );
      }
      for (const fn of iface.functions) {
        if (fn.annotations.capability && !KNOWN_CAPS.has(fn.annotations.capability))
          throw new Error(
            `Unknown capability "${fn.annotations.capability}" in ${pkg.name}/${iface.name}.${fn.name}`
          );
        if (fn.annotations.lease && !fn.annotations.capability)
          throw new Error(
            `@lease without @capability in ${pkg.name}/${iface.name}.${fn.name}`
          );
      }
    }
  }

  // Validate type references resolve
  function checkType(t: TypeNode, ctx: string) {
    if (t.kind === "ref" && !PRIMITIVES.has(t.name!) && t.name !== "handle" && !allRecords.has(t.name!))
      throw new Error(`${ctx}: unknown type "${t.name}"`);
    if (t.inner) checkType(t.inner, ctx);
    if (t.ok) checkType(t.ok, ctx);
    if (t.err) checkType(t.err, ctx);
    if (t.fields) t.fields.forEach((f) => checkType(f, ctx));
  }

  for (const pkg of ast.packages) {
    for (const iface of pkg.interfaces) {
      for (const fn of iface.functions) {
        for (const p of fn.params) checkType(p.type, `${pkg.name}/${iface.name}.${fn.name}(${p.name})`);
        if (fn.returns) checkType(fn.returns, `${pkg.name}/${iface.name}.${fn.name} ret`);
      }
    }
  }
}

// ── Main ───────────────────────────────────────────────────────────────────

const API_DIR = join(import.meta.dir, "..", "..", "..", "api");
const OUT_PATH = join(API_DIR, "build", "nema-api.json");

async function main() {
  const checkOnly = process.argv.includes("--check");

  const entries = await readdir(API_DIR).catch(() => { throw new Error(`api/ directory not found at ${API_DIR}`); });
  const pidlFiles = entries
    .filter((f) => f.endsWith(".pidl"))
    .sort();

  if (pidlFiles.length === 0) {
    throw new Error(`No .pidl files found in ${API_DIR}`);
  }

  const packages: PidlPackage[] = [];

  for (const fname of pidlFiles) {
    const path = join(API_DIR, fname);
    const source = await readFile(path, "utf-8");
    const pkg = parseOneFile(source, { file: fname, lineNo: 0 });
    packages.push(pkg);
    console.log(`  parsed ${fname} → package ${pkg.name} (${pkg.interfaces.length} interfaces, ${pkg.records.length} records)`);
  }

  const ast: PidlAst = { version: "1.0", packages };

  // Validate
  const errors: string[] = [];
  try {
    validate(ast);
  } catch (e: any) {
    errors.push(e.message);
  }

  if (errors.length > 0) {
    console.error("\n❌ Validation errors:");
    for (const e of errors) console.error(`  - ${e}`);
    process.exit(1);
  }

  console.log(`\n  ✓ validated (${packages.length} packages, ${packages.reduce((s, p) => s + p.interfaces.length, 0)} interfaces, ${packages.reduce((s, p) => s + p.records.length, 0)} records)`);

  if (!checkOnly) {
    await writeFile(OUT_PATH, JSON.stringify(ast, null, 2) + "\n");
    console.log(`  ✓ wrote ${OUT_PATH}\n`);
  } else {
    console.log(`\n  (--check mode: no file written)\n`);
  }
}

main().catch((e) => {
  console.error(`\n❌ ${e.message}`);
  process.exit(1);
});
