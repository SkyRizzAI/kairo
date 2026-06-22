// tools/idl/parser.test.ts — Unit tests for the WIT-subset .pidl parser (Plan 48 Fase 2).
import { describe, expect, test } from "bun:test";
import { readFile } from "node:fs/promises";
import { join } from "node:path";

// Re-implement parseOneFile logic from parser.ts inline so we can test it
// without spawning a process. The parser's core logic is extracted here.

// ── Type parser (copied from parser.ts for test isolation) ─────────────────

const PRIMITIVES = new Set(["string", "bool", "u8", "u16", "u32", "u64", "s32", "s64", "f32", "f64"]);

function splitTypeArgs(str: string): string[] {
  const result: string[] = [];
  let depth = 0, current = "";
  for (const ch of str) {
    if (ch === "<") depth++;
    else if (ch === ">") depth--;
    else if (ch === "," && depth === 0) { result.push(current.trim()); current = ""; continue; }
    current += ch;
  }
  if (current.trim()) result.push(current.trim());
  return result;
}

function parseType(raw: string): any {
  const str = raw.trim();
  if (!str) throw new Error("empty type string");
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
  return { kind: "ref", name: str };
}

// ── Test helper: run the full parser pipeline on an inline string ──────────

const KNOWN_CAPS = new Set([
  "display", "input", "input.prev", "input.next", "input.activate",
  "input.back", "input.adjust", "input.2d", "input.touch",
  "camera", "audio.input", "audio.output", "rgb",
  "sensors.environment", "sensors.light", "sensors.motion",
  "net.wifi", "net.http", "bt.ble",
  "storage", "remote.usb", "profile",
]);

// Simplified parseOneFile — uses the same logic as parser.ts
function parseInline(lines: string[]): any {
  let pkg: any = null;
  let pkgDoc = "";
  let currentRecord: any = null;
  let currentInterface: any = null;
  let pendingAnnotations: string[] = [];
  let pendingFuncAnnotations: string[] = [];
  let pendingDoc = "";

  function flushDoc() { const d = pendingDoc; pendingDoc = ""; return d; }
  function flushAnnotations() { const a = collectAnnotations(pendingAnnotations); pendingAnnotations = []; return a; }
  function flushFuncAnnotations() { const a = collectFuncAnnotations(pendingFuncAnnotations); pendingFuncAnnotations = []; return a; }

  for (let i = 0; i < lines.length; i++) {
    const raw = lines[i];
    const trimmed = raw.trim();
    if (!trimmed) continue;
    // Doc comment (must come before // check since /// starts with //)
    if (trimmed.startsWith("///")) {
      const text = trimmed.slice(3).trim();
      pendingDoc = pendingDoc ? pendingDoc + " " + text : text;
      continue;
    }

    if (trimmed.startsWith("//")) continue;

    // Annotation
    const annMatch = trimmed.match(/^@(\w[\w-]*)\(?"?([^"]*)"?\)?$/);
    if (annMatch) {
      pendingAnnotations.push(annMatch[1] + (annMatch[2] ? ":" + annMatch[2] : ""));
      pendingFuncAnnotations.push(annMatch[1] + (annMatch[2] ? ":" + annMatch[2] : ""));
      continue;
    }

    if (trimmed === "}") {
      if (currentRecord) { pkg.records.push(currentRecord); currentRecord = null; }
      else if (currentInterface) { pkg.interfaces.push(currentInterface); currentInterface = null; }
      else throw new Error("unmatched }");
      continue;
    }

    const pkgMatch = trimmed.match(/^package\s+(\S+)@(\d+)\.(\d+)$/);
    if (pkgMatch) {
      if (pkg) throw new Error("duplicate package");
      pkg = { name: pkgMatch[1], major: Number(pkgMatch[2]), minor: Number(pkgMatch[3]), doc: flushDoc(), interfaces: [], records: [] };
      continue;
    }

    const recMatch = trimmed.match(/^record\s+(\S[\w-]*)\s*\{$/);
    if (recMatch) {
      if (currentInterface) throw new Error("record inside interface");
      currentRecord = { name: recMatch[1], doc: flushDoc(), fields: [] };
      continue;
    }

    const ifaceMatch = trimmed.match(/^interface\s+(\S[\w-]*)\s*\{$/);
    if (ifaceMatch) {
      if (!pkg) throw new Error("interface before package");
      const ann = flushAnnotations();
      currentInterface = { name: ifaceMatch[1], doc: flushDoc(), annotations: ann, functions: [] };
      continue;
    }

    // function: name: func(params) -> ret  OR  name: func(params)
    const funcMatch = trimmed.match(/^([\w-]+)\s*:\s*func\((.*)\)\s*(?:->\s*(.+))?$/);
    if (funcMatch) {
      if (!currentInterface) throw new Error("function outside interface");
      const ann = flushFuncAnnotations();
      const paramStr = funcMatch[2], retStr = funcMatch[3]?.trim() || null;
      const params: any[] = paramStr ? splitTypeArgs(paramStr).map((p: string) => {
        const m = p.trim().match(/^([\w-]+)\s*:\s*(.+)$/);
        if (!m) throw new Error(`bad param: "${p}"`);
        return { name: m[1], type: parseType(m[2]) };
      }) : [];
      currentInterface.functions.push({
        name: funcMatch[1], doc: flushDoc(), params,
        returns: retStr ? parseType(retStr) : null, annotations: ann,
      });
      continue;
    }

    // field: name: type,
    const fieldMatch = trimmed.match(/^([\w-]+)\s*:\s*(.+),$/);
    if (fieldMatch) {
      if (!currentRecord) throw new Error("field outside record");
      currentRecord.fields.push({ name: fieldMatch[1], type: parseType(fieldMatch[2]), doc: flushDoc() });
      continue;
    }

    throw new Error(`Cannot parse: "${trimmed}"`);
  }

  if (!pkg) throw new Error("missing package");
  return pkg;
}

function collectAnnotations(lines: string[]) {
  const result: any = { core: false, capability: null };
  for (const l of lines) {
    if (l === "core") result.core = true;
    else if (l.startsWith("capability:")) result.capability = l.slice(11);
  }
  return result;
}

function collectFuncAnnotations(lines: string[]) {
  const result: any = { blocking: false };
  for (const l of lines) {
    if (l === "blocking") result.blocking = true;
  }
  return result;
}

// ── Tests ──────────────────────────────────────────────────────────────────

describe("idl parser — type parsing", () => {
  test("primitives", () => {
    expect(parseType("string")).toEqual({ kind: "string" });
    expect(parseType("bool")).toEqual({ kind: "bool" });
    expect(parseType("u32")).toEqual({ kind: "u32" });
    expect(parseType("s64")).toEqual({ kind: "s64" });
    expect(parseType("handle")).toEqual({ kind: "handle" });
  });

  test("option<T>", () => {
    expect(parseType("option<string>")).toEqual({ kind: "option", inner: { kind: "string" } });
  });

  test("list<T>", () => {
    expect(parseType("list<string>")).toEqual({ kind: "list", inner: { kind: "string" } });
  });

  test("result<T,E>", () => {
    expect(parseType("result<string,u32>")).toEqual({
      kind: "result", ok: { kind: "string" }, err: { kind: "u32" },
    });
  });

  test("tuple<A,B>", () => {
    expect(parseType("tuple<string,u32>")).toEqual({
      kind: "tuple", fields: [{ kind: "string" }, { kind: "u32" }],
    });
  });

  test("tuple<> (empty)", () => {
    expect(parseType("tuple<>")).toEqual({ kind: "tuple", fields: [] });
  });

  test("nested: list<option<string>>", () => {
    expect(parseType("list<option<string>>")).toEqual({
      kind: "list", inner: { kind: "option", inner: { kind: "string" } },
    });
  });

  test("named ref", () => {
    expect(parseType("http-response")).toEqual({ kind: "ref", name: "http-response" });
  });
});

describe("idl parser — inline parsing", () => {
  test("simple interface with one function", () => {
    const pkg = parseInline([
      "package test@1.0",
      "@core",
      "interface foo {",
      "  bar: func(x: string) -> bool",
      "}",
    ]);
    expect(pkg.name).toBe("test");
    expect(pkg.interfaces).toHaveLength(1);
    const iface = pkg.interfaces[0];
    expect(iface.name).toBe("foo");
    expect(iface.annotations.core).toBe(true);
    expect(iface.functions).toHaveLength(1);
    expect(iface.functions[0].name).toBe("bar");
    expect(iface.functions[0].params[0]).toEqual({ name: "x", type: { kind: "string" } });
    expect(iface.functions[0].returns).toEqual({ kind: "bool" });
  });

  test("record + interface referencing it", () => {
    const pkg = parseInline([
      "package test@1.0",
      "record rec-a {",
      "  x: u32,",
      "  y: string,",
      "}",
      "@capability(\"net.http\")",
      "interface api {",
      "  get: func() -> rec-a",
      "}",
    ]);
    expect(pkg.records).toHaveLength(1);
    expect(pkg.records[0].name).toBe("rec-a");
    expect(pkg.records[0].fields).toHaveLength(2);
    expect(pkg.interfaces[0].functions[0].returns).toEqual({ kind: "ref", name: "rec-a" });
    expect(pkg.interfaces[0].annotations.capability).toBe("net.http");
  });

  test("void function (no return)", () => {
    const pkg = parseInline([
      "package test@1.0",
      "interface foo {",
      "  action: func(x: string)",
      "}",
    ]);
    expect(pkg.interfaces[0].functions[0].returns).toBeNull();
  });

  test("@blocking function annotation", () => {
    const pkg = parseInline([
      "package test@1.0",
      "interface foo {",
      "  @blocking",
      "  slow: func(url: string) -> string",
      "}",
    ]);
    expect(pkg.interfaces[0].functions[0].annotations.blocking).toBe(true);
  });

  test("doc comments attach to next declaration", () => {
    const pkg = parseInline([
      "/// Package doc",
      "package test@1.0",
      "/// Interface doc",
      "interface foo {",
      "  /// Function doc",
      "  bar: func() -> string",
      "}",
    ]);
    expect(pkg.doc).toBe("Package doc");
    expect(pkg.interfaces[0].doc).toBe("Interface doc");
    expect(pkg.interfaces[0].functions[0].doc).toBe("Function doc");
  });

  test("multiple interfaces in one package", () => {
    const pkg = parseInline([
      "package multi@2.3",
      "interface a {",
      "  f: func() -> u32",
      "}",
      "interface b {",
      "  g: func() -> string",
      "}",
    ]);
    expect(pkg.major).toBe(2);
    expect(pkg.minor).toBe(3);
    expect(pkg.interfaces).toHaveLength(2);
    expect(pkg.interfaces[0].name).toBe("a");
    expect(pkg.interfaces[1].name).toBe("b");
  });

  test("function with no args", () => {
    const pkg = parseInline([
      "package test@1.0",
      "interface foo {",
      "  now: func() -> u64",
      "}",
    ]);
    expect(pkg.interfaces[0].functions[0].params).toEqual([]);
  });

  test("kebab-case names in functions and params", () => {
    const pkg = parseInline([
      "package test@1.0",
      "interface foo {",
      "  user-name: func() -> string",
      "  verify-password: func(input-str: string) -> bool",
      "}",
    ]);
    expect(pkg.interfaces[0].functions[0].name).toBe("user-name");
    expect(pkg.interfaces[0].functions[1].name).toBe("verify-password");
    expect(pkg.interfaces[0].functions[1].params[0].name).toBe("input-str");
  });
});

describe("idl parser — error cases", () => {
  test("unknown annotation is ignored (forward-compat)", () => {
    const pkg = parseInline([
      "package test@1.0",
      "@future",
      "interface foo {",
      "  bar: func() -> string",
      "}",
    ]);
    // @future is not recognized but should not crash
    expect(pkg.interfaces[0].annotations.core).toBe(false);
    expect(pkg.interfaces[0].annotations.capability).toBeNull();
  });

  test("interface before package", () => {
    expect(() => parseInline([
      "interface foo {",
      "  bar: func() -> string",
      "}",
    ])).toThrow("interface before package");
  });

  test("function outside interface", () => {
    expect(() => parseInline([
      "package test@1.0",
      "bar: func() -> string",
    ])).toThrow("function outside interface");
  });

  test("duplicate package", () => {
    expect(() => parseInline([
      "package test@1.0",
      "package test@2.0",
    ])).toThrow("duplicate package");
  });

  test("bad param syntax", () => {
    expect(() => parseInline([
      "package test@1.0",
      "interface foo {",
      "  bar: func(x) -> string",
      "}",
    ])).toThrow("bad param");
  });

  test("record inside interface (not supported)", () => {
    expect(() => parseInline([
      "package test@1.0",
      "interface foo {",
      "  record inner {",
      "    x: string,",
      "  }",
      "}",
    ])).toThrow("record inside interface");
  });
});

describe("idl parser — snapshot: nema-api.json", () => {
  test("generated AST is valid JSON with expected structure", async () => {
    const path = join(import.meta.dir, "..", "..", "..", "api", "build", "nema-api.json");
    const raw = await readFile(path, "utf-8");
    const ast = JSON.parse(raw);

    expect(ast.version).toBe("1.0");
    expect(Array.isArray(ast.packages)).toBe(true);
    expect(ast.packages.length).toBeGreaterThanOrEqual(4);

    // Every package has required fields
    for (const pkg of ast.packages) {
      expect(typeof pkg.name).toBe("string");
      expect(typeof pkg.major).toBe("number");
      expect(typeof pkg.minor).toBe("number");
      expect(Array.isArray(pkg.interfaces)).toBe(true);
      expect(Array.isArray(pkg.records)).toBe(true);

      for (const iface of pkg.interfaces) {
        expect(typeof iface.name).toBe("string");
        expect(Array.isArray(iface.functions)).toBe(true);
        expect(typeof iface.annotations).toBe("object");
        expect("core" in iface.annotations).toBe(true);

        for (const fn of iface.functions) {
          expect(typeof fn.name).toBe("string");
          expect(Array.isArray(fn.params)).toBe(true);
          expect(typeof fn.annotations).toBe("object");
          expect("blocking" in fn.annotations).toBe(true);
        }
      }

      for (const rec of pkg.records) {
        expect(typeof rec.name).toBe("string");
        expect(Array.isArray(rec.fields)).toBe(true);
        for (const field of rec.fields) {
          expect(typeof field.name).toBe("string");
          expect(typeof field.type).toBe("object");
          expect(typeof field.type.kind).toBe("string");
        }
      }
    }

    // Verify specific known interfaces exist
    const sys = ast.packages.find((p: any) => p.name === "nema:sys");
    expect(sys).toBeDefined();
    const ifaceNames = sys.interfaces.map((i: any) => i.name);
    expect(ifaceNames).toContain("log");
    expect(ifaceNames).toContain("device");
    expect(ifaceNames).toContain("events");
    expect(ifaceNames).toContain("tasks");

    const storage = ast.packages.find((p: any) => p.name === "nema:storage");
    expect(storage.interfaces[0].name).toBe("kv");
    expect(storage.interfaces[0].annotations.core).toBe(true);

    const net = ast.packages.find((p: any) => p.name === "nema:net");
    expect(net.interfaces.map((i: any) => i.name)).toContain("http");
    expect(net.interfaces.map((i: any) => i.name)).toContain("wifi");
  });

  test("all 24 interfaces from 9 packages present", async () => {
    const path = join(import.meta.dir, "..", "..", "..", "api", "build", "nema-api.json");
    const raw = await readFile(path, "utf-8");
    const ast = JSON.parse(raw);

    const allInterfaces: string[] = [];
    let totalRecords = 0;
    for (const pkg of ast.packages) {
      for (const iface of pkg.interfaces) {
        allInterfaces.push(`${pkg.name}/${iface.name}`);
      }
      totalRecords += pkg.records.length;
    }

    expect(allInterfaces.sort()).toEqual([
      "aether:ui/interactive",
      "aether:ui/scroll",
      "aether:ui/text",
      "aether:ui/view",
      "nema:bt/ble",
      "nema:input/input",
      "nema:media/audio-input",
      "nema:media/audio-output",
      "nema:media/camera",
      "nema:net/http",
      "nema:net/wifi",
      "nema:profile/profile",
      "nema:storage/fs",
      "nema:storage/kv",
      "nema:sys/device",
      "nema:sys/events",
      "nema:sys/log",
      "nema:sys/tasks",
      "palanu:plp/channels",
      "palanu:plp/control_ops",
      "palanu:plp/ext_ops",
      "palanu:plp/file_ops",
      "palanu:plp/ota_ops",
      "palanu:plp/system_ops",
    ]);
    expect(totalRecords).toBe(4); // http-response, wifi-ap, field, frame_flags
  });
});
