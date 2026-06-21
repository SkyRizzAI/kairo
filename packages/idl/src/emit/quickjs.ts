// tools/idl/emit/quickjs.ts — Generate QuickJS binding glue (Plan 49 Fase 2).
// Emits generated/host/nema_api_quickjs.gen.cpp — replaces hand-written js_api.cpp.

import type { PidlAst, PidlFunc, TypeNode } from "../ast";
import { typeToString } from "../ast";

// ── Helpers ────────────────────────────────────────────────────────────────

function pascalCase(s: string): string { return s.split("-").map(w => w.charAt(0).toUpperCase() + w.slice(1)).join(""); }
function camelCase(s: string): string { const p = pascalCase(s); return p.charAt(0).toLowerCase() + p.slice(1); }
function methodName(ifaceName: string, funcName: string): string { return `${ifaceName}_${funcName}`.replace(/-/g, "_"); }
function cppName(func: PidlFunc, iface: string): string { return `nema_${iface}_${func.name}`.replace(/-/g, "_"); }

// Simplified C++ param type for unmarshalling variable declarations
function cppParamType2(t: TypeNode): string {
  switch (t.kind) {
    case "string": return "std::string";
    case "bool":   return "bool";
    case "u8": case "u16": case "u32": case "s32": case "handle": return "int32_t";
    case "s64": case "u64": return "int64_t";
    case "f32": case "f64": return "double";
    case "list": return `std::vector<${cppParamType2(t.inner!)}>`;
    case "option": return `std::optional<${cppParamType2(t.inner!)}>`;
    case "ref": return pascalCase(t.name!);
    case "result": return `NemaResult<${cppParamType2(t.ok!)}, ${cppParamType2(t.err!)}>`;
    case "tuple": return "int"; // stub
    default: return "int";
  }
}

// ── QuickJS marshalling helpers ────────────────────────────────────────────

function qjsUnmarshal(t: TypeNode, expr: string): string {
  switch (t.kind) {
    case "string": return `jsToString(ctx, ${expr})`;
    case "bool":   return `JS_ToBool(ctx, ${expr})`;
    case "u8": case "u16": case "u32": return `jsToU32(ctx, ${expr})`;
    case "s32": return `jsToI32(ctx, ${expr})`;
    case "s64": return `jsToI64(ctx, ${expr})`;
    case "u64": return `jsToU64(ctx, ${expr})`;
    case "handle": return `jsToI32(ctx, ${expr})`;
    case "f32": case "f64": return `jsToDouble(ctx, ${expr})`;
    // Complex types that can't be unmarshalled from JS yet — return default.
    case "list":   return `{}`;
    case "ref":    return `{}`;
    case "option": return `std::nullopt`;
    case "result": return `{}`;
    case "tuple":  return `{}`;
    default: return `{}`;
  }
}

function qjsMarshal(t: TypeNode, cppExpr: string): string {
  switch (t.kind) {
    case "string": return `JS_NewString(ctx, ${cppExpr}.c_str())`;
    case "bool":   return `JS_NewBool(ctx, ${cppExpr})`;
    case "u8": case "u16": case "u32": case "s32": return `JS_NewInt32(ctx, ${cppExpr})`;
    case "s64": return `JS_NewInt64(ctx, ${cppExpr})`;
    case "u64": return `JS_NewInt64(ctx, ${cppExpr})`;
    case "f32": case "f64": return `JS_NewFloat64(ctx, ${cppExpr})`;
    case "handle": return `JS_NewInt32(ctx, ${cppExpr})`;
    case "option": {
      const inner = qjsMarshal(t.inner!, `v`);
      return `marshalOption(ctx, ${cppExpr}, [ctx](JSContext* c, auto& v) { (void)c; return ${inner}; })`;
    }
    case "list": {
      const inner = qjsMarshal(t.inner!, `v`);
      return `marshalList(ctx, ${cppExpr}, [ctx](JSContext* c, auto& v) { (void)c; return ${inner}; })`;
    }
    case "result": {
      const okVal = qjsMarshal(t.ok!, "v");
      const errVal = qjsMarshal(t.err!, "e");
      if (t.ok!.kind === "tuple" && (!t.ok!.fields || t.ok!.fields.length === 0)) {
        return `marshalResultVoid(ctx, ${cppExpr}, [ctx](JSContext* c, auto& e) { (void)c; return ${errVal}; })`;
      }
      return `marshalResult(ctx, ${cppExpr}, [ctx](JSContext* c, auto& v) { (void)c; return ${okVal}; }, [ctx](JSContext* c, auto& e) { (void)c; return ${errVal}; })`;
    }
    case "ref": return `marshalRecord(ctx, ${cppExpr})`;
    case "tuple": {
      if (!t.fields || t.fields.length === 0) return "JS_UNDEFINED";
      return `marshalTuple(ctx, ${cppExpr})`;
    }
    default: return "JS_UNDEFINED";
  }
}

// ── Emit one function wrapper ──────────────────────────────────────────────

function emitFuncWrapper(func: PidlFunc, iface: string): string {
  const fnName = cppName(func, iface);
  const hostMethod = methodName(iface, func.name);
  const lines: string[] = [];

  lines.push(`// ${iface}.${func.name}`);
  if (func.annotations.blocking) {
    lines.push("// @blocking — dispatched via TaskRunner (the host wraps as async).");
  }
  lines.push(`static JSValue ${fnName}(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {`);
  lines.push("    (void)argc; (void)argv;");
  lines.push("    auto* e = engineOf(ctx);");
  lines.push("    auto* host = e->hostApi();");
  lines.push("    if (!host) return JS_UNDEFINED;");
  lines.push("");

  // Unmarshal params
  for (let i = 0; i < func.params.length; i++) {
    const p = func.params[i];
    const cppVar = p.name.replace(/-/g, "_");
    const expr = qjsUnmarshal(p.type, `argv[${i}]`);
    if (p.type.kind === "string") {
      lines.push(`    std::string ${cppVar} = ${expr};`);
    } else if (p.type.kind === "list" || p.type.kind === "ref" || p.type.kind === "option" || p.type.kind === "result" || p.type.kind === "tuple") {
      // Complex types need explicit C++ type (auto can't deduce from {})
      const cppT = cppParamType2(p.type);
      lines.push(`    ${cppT} ${cppVar} = ${expr};`);
    } else {
      lines.push(`    auto ${cppVar} = ${expr};`);
    }
  }

  // Call host method
  const paramNames = func.params.map(p => p.name.replace(/-/g, "_")).join(", ");
  const callExpr = `host->${hostMethod}(${paramNames})`;

  if (!func.returns) {
    lines.push(`    ${callExpr};`);
    lines.push("    return JS_UNDEFINED;");
  } else {
    lines.push(`    auto __ret = ${callExpr};`);
    const retExpr = qjsMarshal(func.returns, "__ret");
    lines.push(`    return ${retExpr};`);
  }

  lines.push("}");
  return lines.join("\n");
}

// ── Emit registration ─────────────────────────────────────────────────────

function emitRegistration(ast: PidlAst): string {
  const lines: string[] = [];
  lines.push("// Build the `nema` global namespace tree + register all C functions.");
  lines.push("// Namespace structure matches the IDL (Plan 48 Fase 4 canonical paths).");
  lines.push("void installNemaApi(JSContext* ctx, HostApi* host, nema::CapabilityRegistry& caps) {");
  lines.push("    JSValue g = JS_GetGlobalObject(ctx);");
  lines.push("    JSValue nema = JS_NewObject(ctx);");
  lines.push("");

  // Fixed namespace tree. Each leaf is an object that holds C functions.
  const tree: Record<string, { parent: string; var: string; gated?: string }> = {
    "sys":          { parent: "nema",      var: "sys" },
    "sys.log":      { parent: "sys",       var: "sys_log" },
    "sys.device":   { parent: "sys",       var: "sys_device" },
    "sys.events":   { parent: "sys",       var: "sys_events" },
    "sys.tasks":    { parent: "sys",       var: "sys_tasks" },
    "storage":      { parent: "nema",      var: "storage" },
    "storage.kv":   { parent: "storage",   var: "storage_kv" },
    "storage.fs":   { parent: "storage",   var: "storage_fs" },
    "net":          { parent: "nema",      var: "net" },
    "net.http":     { parent: "net",       var: "net_http", gated: "net.http" },
    "net.wifi":     { parent: "net",       var: "net_wifi", gated: "net.wifi" },
    "profile":      { parent: "nema",      var: "profile",  gated: "profile" },
    "bt":           { parent: "nema",      var: "bt" },
    "bt.ble":       { parent: "bt",        var: "bt_ble",   gated: "bt.ble" },
    "media":        { parent: "nema",      var: "media" },
    "media.audio-input":  { parent: "media", var: "media_audio_input", gated: "audio.input" },
    "media.audio-output": { parent: "media", var: "media_audio_output", gated: "audio.output" },
    "media.camera": { parent: "media",      var: "media_camera", gated: "camera" },
    "input":        { parent: "nema",      var: "input",    gated: "input" },
  };

  // Build namespace objects (only those present in the AST)
  // Warn loudly if an IDL interface is missing from the tree — catches exactly
  // the bug where we added storage.fs to the PIDL but forgot to wire it here.
  const built = new Set<string>();
  for (const pkg of ast.packages) {
    const domain = pkg.name.split(":")[1] || pkg.name;
    for (const iface of pkg.interfaces) {
      const path = `${domain}.${iface.name}`;
      const leaf = `${domain}/${iface.name}`;
      const info = tree[path];
      if (!info) {
        // ui.* is handled by the aether display server, not the nema global.
        // input.input and profile.profile use a different wiring path.
        const knownExclusions = new Set(["ui.view","ui.text","ui.interactive","ui.scroll","input.input","profile.profile"]);
        if (!knownExclusions.has(path))
          console.warn(`[quickjs] WARNING: interface '${path}' not in tree map — it will NOT be exposed to JS! Add it to the tree.`);
        continue;
      }

      // Ensure parent exists
      const parts = path.split(".");
      for (let i = 0; i < parts.length; i++) {
        const ns = parts.slice(0, i + 1).join(".");
        if (!built.has(ns) && tree[ns]) {
          built.add(ns);
          const ni = tree[ns];
          if (ni.gated) {
            const cap = ni.gated;
            lines.push(`    JSValue ${ni.var} = JS_NewObject(ctx);`);
            lines.push(`    if (caps.has(nema::caps::${capConstName(cap)})) {`);
            lines.push(`        JS_SetPropertyStr(ctx, ${ni.parent}, "${parts[i]}", ${ni.var});`);
            lines.push("    }");
          } else {
            lines.push(`    JSValue ${ni.var} = JS_NewObject(ctx);`);
            lines.push(`    JS_SetPropertyStr(ctx, ${ni.parent}, "${parts[i]}", ${ni.var});`);
          }
        }
      }

      // Register functions on this interface's namespace object
      const leafVar = info.var;
      for (const fn of iface.functions) {
        const fnName = cppName(fn, iface.name);
        lines.push(`    setFn(ctx, ${leafVar}, "${camelCase(fn.name)}", ${fnName}, ${fn.params.length});`);
      }
      lines.push("");
    }
  }

  // Device name + caps (special static properties, not C functions)
  if (built.has("sys.device")) {
    lines.push("    // device.name + device.caps (static properties)");
    lines.push("    JS_SetPropertyStr(ctx, sys_device, \"name\", JS_NewString(ctx, host->device_name().c_str()));");
    lines.push("    {");
    lines.push("        const auto& cl = host->device_caps();");
    lines.push("        JSValue arr = JS_NewArray(ctx);");
    lines.push("        for (uint32_t i = 0; i < cl.size(); i++)");
    lines.push("            JS_SetPropertyUint32(ctx, arr, i, JS_NewString(ctx, cl[i].c_str()));");
    lines.push("        JS_SetPropertyStr(ctx, sys_device, \"caps\", arr);");
    lines.push("    }");
    lines.push("");
  }

  lines.push("    JS_SetPropertyStr(ctx, g, \"nema\", nema);");
  lines.push("    JS_FreeValue(ctx, g);");
  lines.push("}");

  return lines.join("\n");
}

// Capability string → nema::caps::XxxYyy constant name
function capConstName(cap: string): string {
  // Direct mapping to actual constants in capabilities.h (Plan 42)
  const MAP: Record<string, string> = {
    "display":        "Display",
    "input":          "Input",
    "input.prev":     "InputPrev",
    "input.next":     "InputNext",
    "input.activate": "InputActivate",
    "input.back":     "InputBack",
    "input.adjust":   "InputAdjust",
    "input.2d":       "Input2D",
    "input.touch":    "InputTouch",
    "camera":         "Camera",
    "audio.input":    "AudioInput",
    "audio.output":   "AudioOutput",
    "rgb":            "Rgb",
    "sensors.environment": "SensorsEnv",
    "sensors.light":  "SensorsLight",
    "sensors.motion": "SensorsMotion",
    "net.wifi":       "NetWifi",
    "net.http":       "NetHttp",
    "bt.ble":         "BtBle",
    "storage":        "Storage",
    "remote.usb":     "RemoteUsb",
    "profile":        "Profile",
  };
  return MAP[cap] || pascalCase(cap.replace(/\./g, "_"));
}

// ── QuickJS marshalling helpers (injected at top of generated file) ──────

function emitHelpers(): string {
  return `// QuickJS ↔ C++ marshalling helpers (generated once, shared by all wrappers).
// These are deliberately simple wrappers; the generator emits calls to them
// rather than inlining the marshalling for every function.

static JsEngine* engineOf(JSContext* ctx) {
    return static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
}

static std::string jsToString(JSContext* ctx, JSValueConst v) {
    std::string r;
    const char* s = JS_ToCString(ctx, v);
    if (s) { r = s; JS_FreeCString(ctx, s); }
    return r;
}

static int32_t jsToI32(JSContext* ctx, JSValueConst v) {
    int32_t r = 0;
    JS_ToInt32(ctx, &r, v);
    return r;
}

static uint32_t jsToU32(JSContext* ctx, JSValueConst v) {
    uint32_t r = 0;
    JS_ToUint32(ctx, &r, v);
    return r;
}

static int64_t jsToI64(JSContext* ctx, JSValueConst v) {
    int64_t r = 0;
    JS_ToInt64(ctx, &r, v);
    return r;
}

static uint64_t jsToU64(JSContext* ctx, JSValueConst v) {
    int64_t r = 0;
    JS_ToInt64(ctx, &r, v);
    return static_cast<uint64_t>(r);
}

static double jsToDouble(JSContext* ctx, JSValueConst v) {
    double r = 0;
    JS_ToFloat64(ctx, &r, v);
    return r;
}

static void setFn(JSContext* ctx, JSValue obj, const char* name, JSCFunction* fn, int argc) {
    JS_SetPropertyStr(ctx, obj, name, JS_NewCFunction(ctx, fn, name, argc));
}

// Marshal std::optional<T> → JS (null or wrapped value)
template <typename T, typename F>
static JSValue marshalOption(JSContext* ctx, const std::optional<T>& opt, F wrap) {
    if (!opt.has_value()) return JS_NULL;
    return wrap(ctx, opt.value());
}

// Marshal std::vector<T> → JS Array
template <typename T, typename F>
static JSValue marshalList(JSContext* ctx, const std::vector<T>& vec, F wrap) {
    JSValue arr = JS_NewArray(ctx);
    for (uint32_t i = 0; i < vec.size(); i++)
        JS_SetPropertyUint32(ctx, arr, i, wrap(ctx, vec[i]));
    return arr;
}

// Marshal NemaResult<T,E> → JS (ok object or throw)
template <typename T, typename E, typename FW, typename FE>
static JSValue marshalResult(JSContext* ctx, const NemaResult<T,E>& r, FW wrapOk, FE wrapErr) {
    if (r.ok) return wrapOk(ctx, r.value);
    return JS_ThrowTypeError(ctx, r.error.c_str());
}

template <typename E, typename FE>
static JSValue marshalResultVoid(JSContext* ctx, const NemaResult<void,E>& r, FE wrapErr) {
    if (r.ok) return JS_NewObject(ctx);
    return JS_ThrowTypeError(ctx, r.error.c_str());
}

// Stub marshalling for records + tuples (used by @future interfaces).
// Full marshalling will be generated in a later phase.
template <typename T>
static JSValue marshalRecord(JSContext* ctx, const T&) {
    (void)ctx;
    return JS_UNDEFINED;
}
template <typename... Ts>
static JSValue marshalTuple(JSContext* ctx, const std::tuple<Ts...>&) {
    (void)ctx;
    return JS_UNDEFINED;
}
`;
}

// ── Main emitter ───────────────────────────────────────────────────────────

export function emitQuickJS(ast: PidlAst): string {
  const lines: string[] = [];

  lines.push("#include \"nema/js/js_engine.h\"");
  lines.push("#include \"host/nema_api.gen.h\"");
  lines.push("#include \"nema/system/capabilities.h\"");
  lines.push("#include \"nema/system/capability_registry.h\"");
  lines.push("#include <string>");
  lines.push("#include <vector>");
  lines.push("");
  lines.push("using nema::js::JsEngine;");
  lines.push("");

  lines.push("namespace {");
  lines.push("");
  lines.push(emitHelpers());
  lines.push("");

  lines.push("// ── Generated function wrappers ───────────────────────────");
  lines.push("");

  // Emit all function wrappers
  for (const pkg of ast.packages) {
    for (const iface of pkg.interfaces) {
      for (const fn of iface.functions) {
        lines.push(emitFuncWrapper(fn, iface.name));
        lines.push("");
      }
    }
  }

  lines.push("} // anon namespace");
  lines.push("");

  lines.push("// ── Generated registration ────────────────────────────────");
  lines.push("");

  lines.push(emitRegistration(ast));
  lines.push("");

  return lines.join("\n");
}
