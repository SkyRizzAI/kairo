#!/usr/bin/env bun
// kairo-build <appDir> — bundle an app's TSX into a single `.kapp`.
// The `kairo` runtime is EXTERNAL (provided by the device JS host) so the app +
// host share one runtime instance (hooks/render identity). Output container:
//   KAPP1\n<manifest-json>\n<js-bundle>
import { resolve, join } from "path";

const appDir = resolve(Bun.argv[2] ?? ".");
const manifestPath = join(appDir, "kapp.json");
const manifest = JSON.parse(await Bun.file(manifestPath).text());
const entry = join(appDir, manifest.entry ?? "App.tsx");

const result = await Bun.build({
  entrypoints: [entry],
  target: "browser",
  format: "esm",
  minify: true,
  external: ["kairo", "kairo/jsx-runtime", "kairo/jsx-dev-runtime"],
});

if (!result.success) {
  console.error("build failed:");
  for (const log of result.logs) console.error(log);
  process.exit(1);
}

const js = await result.outputs[0].text();
const outPath = join(appDir, `${manifest.id ?? "app"}.kapp`);
await Bun.write(outPath, `KAPP1\n${JSON.stringify(manifest)}\n${js}`);
console.log(`✓ built ${outPath}`);
console.log(`  app=${manifest.name} id=${manifest.id} js=${js.length}B needs=${(manifest.needs ?? []).join(",") || "-"}`);
