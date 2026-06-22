// @palanu/app-sdk build tool — bundle a TSX app into a .papp folder.
//
// Usage:
//   bun run app:build              # auto-detect manifest.json in cwd
//   bun run app:build:hello        # build examples/hello
//   bunx palanu-build --dir path   # specify app directory
//
// Output: dist/<id>.papp/   (macOS .app-style folder ready to install)

import { resolve, join, basename } from "path";
import { mkdir, writeFile, rm, access } from "node:fs/promises";
import type sharp from "sharp";
import type { PappManifest } from "../src/manifest";

// ── Parse args ─────────────────────────────────────────────────────────────
// Accepts: `palanu-build --dir <path>` or `palanu-build <path>` or cwd default.

const dirFlag = Bun.argv.indexOf("--dir");
let appDirRaw = ".";
if (dirFlag >= 0) {
  appDirRaw = Bun.argv[dirFlag + 1];
} else {
  // First non-flag positional arg after the script itself
  const positional = Bun.argv.slice(2).find(a => !a.startsWith("-"));
  if (positional) appDirRaw = positional;
}
const appDir = resolve(appDirRaw);

const manifestPath = join(appDir, "manifest.json");
const manifest: PappManifest = JSON.parse(await Bun.file(manifestPath).text());

// Inject defaults
if (!manifest.api_version) manifest.api_version = "1.0";
if (!manifest.runtime)     manifest.runtime = "js";
if (!manifest.id)          manifest.id = "com.palanu.app";

const entryFile = manifest.entry ?? "App.tsx";
const entry = join(appDir, entryFile);

// ── Write .papp folder ─────────────────────────────────────────────────────

const outDir = join(appDir, "dist", `${manifest.id}.papp`);
await rm(outDir, { recursive: true, force: true });
await mkdir(outDir, { recursive: true });

console.log(`  building ${manifest.name} (${manifest.id})...`);

if (manifest.runtime === "wasm") {
  // ── Build WASM (C → .wasm via wasi-sdk) ─────────────────────────────────
  const wasiSdk = process.env.WASI_SDK_PATH ?? "/opt/wasi-sdk";
  const clang = join(wasiSdk, "bin", "clang");
  if (!(await Bun.file(clang).exists())) {
    console.error(`wasi-sdk not found at ${wasiSdk}`);
    console.error("Set WASI_SDK_PATH or install to /opt/wasi-sdk:");
    console.error("  https://github.com/WebAssembly/wasi-sdk/releases");
    process.exit(1);
  }
  const srcFile = join(appDir, "main.c");
  const outWasm = join(outDir, entryFile);
  const proc = Bun.spawn(
    [clang, "--target=wasm32-wasi", "-O2", "-o", outWasm, srcFile],
    { stdout: "inherit", stderr: "inherit" }
  );
  const code = await proc.exited;
  if (code !== 0) { console.error("clang failed"); process.exit(code ?? 1); }
  console.log(`  ${entryFile}  (${Bun.file(outWasm).size}B)`);
} else {
  // ── Build JS/TSX (via Bun bundler) ───────────────────────────────────────
  const result = await Bun.build({
    entrypoints: [entry],
    target: "browser",
    format: "esm",
    minify: true,
    external: ["nema", "nema/jsx-runtime", "nema/jsx-dev-runtime"],
    loader: { ".tsx": "tsx", ".ts": "ts", ".jsx": "jsx" },
  });

  if (!result.success) {
    console.error("build failed:");
    for (const log of result.logs) console.error(log);
    process.exit(1);
  }

  const js = await result.outputs[0].text();
  const jsName = basename(entryFile).replace(/\.(tsx?|jsx?)$/, ".js");
  await writeFile(join(outDir, jsName), js);
}

// ── Icon pipeline ──────────────────────────────────────────────────────────
// icon.raw format: 4-byte header (width u16le, height u16le) + 1-bit packed
// pixels (MSB first, row-major, stride = ceil(w/8)).  Firmware renders this
// via Icon() node — same code path as the built-in icon_pack.

async function pngToIconRaw(pngPath: string): Promise<Uint8Array | null> {
  let sharpMod: typeof sharp;
  try {
    sharpMod = (await import("sharp")).default as unknown as typeof sharp;
  } catch {
    console.warn("  warn: sharp not available — skipping icon.png → icon.raw");
    console.warn("        run 'bun add sharp' in the SDK directory to enable auto-conversion");
    return null;
  }
  const { data, info } = await (sharpMod as any)(pngPath)
    .grayscale()
    .raw()
    .toBuffer({ resolveWithObject: true });
  const w = info.width as number;
  const h = info.height as number;
  const stride = Math.ceil(w / 8);
  const bitmap = new Uint8Array(stride * h);
  // Dark pixels (lum < 128) → bit set (pixel on), light → bit clear.
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      if (data[y * w + x] < 128) {
        bitmap[y * stride + (x >> 3)] |= 0x80 >> (x & 7);  // MSB first
      }
    }
  }
  const raw = new Uint8Array(4 + bitmap.length);
  raw[0] = w & 0xff;  raw[1] = (w >> 8) & 0xff;
  raw[2] = h & 0xff;  raw[3] = (h >> 8) & 0xff;
  raw.set(bitmap, 4);
  return raw;
}

// Check for icon: prefer icon.raw (pre-converted), fall back to icon.png.
let iconBytes: Uint8Array | null = null;
const iconRawSrc = join(appDir, "icon.raw");
const iconPngSrc = join(appDir, "icon.png");

const rawExists  = await access(iconRawSrc).then(() => true).catch(() => false);
const pngExists  = await access(iconPngSrc).then(() => true).catch(() => false);

if (rawExists) {
  iconBytes = new Uint8Array(await Bun.file(iconRawSrc).arrayBuffer());
  console.log(`  icon.raw  (pre-converted, ${iconBytes.length}B)`);
} else if (pngExists) {
  iconBytes = await pngToIconRaw(iconPngSrc);
  if (iconBytes) {
    const w = iconBytes[0] | (iconBytes[1] << 8);
    const h = iconBytes[2] | (iconBytes[3] << 8);
    console.log(`  icon.png → icon.raw  (${w}×${h}, ${iconBytes.length}B)`);
  }
}

if (iconBytes) {
  await writeFile(join(outDir, "icon.raw"), iconBytes);
  (manifest as any).icon = "icon.raw";
}

// Re-write manifest now that icon field may have been added.
await writeFile(join(outDir, "manifest.json"), JSON.stringify(manifest, null, 2));

const tier = manifest.runtime ?? "js";
const server = manifest.display_server ?? "any";
console.log(`✓ ${outDir}/`);
console.log(`  manifest.json  ${entryFile}`);
console.log(`  runtime=${tier}  server=${server}`);
console.log("");
console.log("  Install: copy this folder to /apps/ on your device");
