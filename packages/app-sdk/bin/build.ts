// @palanu/app-sdk build tool — bundle a TSX app into a .papp folder.
//
// Usage:
//   bun run app:build              # auto-detect manifest.json in cwd
//   bun run app:build:hello        # build examples/hello
//   bunx palanu-build --dir path   # specify app directory
//
// Output: dist/<id>.papp/   (macOS .app-style folder ready to install)

import { resolve, join, basename } from "path";
import { mkdir, writeFile, rm } from "node:fs/promises";
import type { PappManifest } from "../src/manifest";

// ── Parse args ─────────────────────────────────────────────────────────────

const dirFlag = Bun.argv.indexOf("--dir");
const appDir = resolve(dirFlag >= 0 ? Bun.argv[dirFlag + 1] : ".");

const manifestPath = join(appDir, "manifest.json");
const manifest: PappManifest = JSON.parse(await Bun.file(manifestPath).text());

// Inject defaults
if (!manifest.api_version) manifest.api_version = "1.0";
if (!manifest.runtime)     manifest.runtime = "js";
if (!manifest.id)          manifest.id = "com.palanu.app";

const entryFile = manifest.entry ?? "App.tsx";
const entry = join(appDir, entryFile);

// ── Build JS ───────────────────────────────────────────────────────────────

console.log(`  building ${manifest.name} (${manifest.id})...`);

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

// ── Write .papp folder ─────────────────────────────────────────────────────

const outDir = join(appDir, "dist", `${manifest.id}.papp`);
await rm(outDir, { recursive: true, force: true });
await mkdir(outDir, { recursive: true });

await writeFile(join(outDir, "manifest.json"), JSON.stringify(manifest, null, 2));
await writeFile(join(outDir, jsName), js);

const tier = manifest.runtime ?? "js";
const server = manifest.display_server ?? "any";
console.log(`✓ ${outDir}/`);
console.log(`  manifest.json  app.js (${js.length}B)`);
console.log(`  runtime=${tier}  server=${server}`);
console.log("");
console.log("  Install: copy this folder to /apps/ on your device");
