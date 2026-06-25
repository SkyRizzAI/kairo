// Palanu asset_gen CLI
// Usage:
//   bun run src/index.ts png2c   <input.png> --out <output.h> --sym <SYM>
//   bun run src/index.ts seq2panim <dir/>    --out <output.panim>
//   bun run src/index.ts batch   --config <config.json>

import { png2c, pngSeq2c } from "./png2c.js";
import { seq2panim } from "./seq2panim.js";
import * as fs from "fs";
import * as path from "path";

const [,, cmd, ...rest] = process.argv;

function arg(flag: string, args: string[]): string | undefined {
  const i = args.indexOf(flag);
  return i !== -1 ? args[i + 1] : undefined;
}

async function main() {
  if (cmd === "png2c") {
    const inputPath  = rest[0];
    const outputPath = arg("--out", rest) ?? "out.h";
    const symbolName = arg("--sym", rest) ?? "kIcon";
    const invert     = rest.includes("--invert");
    if (!inputPath) { console.error("Usage: png2c <input.png> --out <output.h> --sym <SYM> [--invert]"); process.exit(1); }
    const { w, h } = await png2c({ inputPath, outputPath, symbolName, invert });
    console.log(`${path.basename(inputPath)} → ${symbolName}  (${w}×${h})`);

  } else if (cmd === "seq2panim") {
    const inputDir   = rest[0];
    const outputPath = arg("--out", rest) ?? "out.panim";
    if (!inputDir) { console.error("Usage: seq2panim <dir/> --out <output.panim>"); process.exit(1); }
    await seq2panim({ inputDir, outputPath });

  } else if (cmd === "batch") {
    const configPath = arg("--config", rest) ?? "asset_gen.config.json";
    const config = JSON.parse(fs.readFileSync(configPath, "utf8")) as {
      png2c?:    Array<{ input: string; output: string; sym: string; invert?: boolean }>;
      seq2panim?: Array<{ input: string; output: string }>;
      seqC?:     Array<{ input: string; output: string; baseSym: string }>;
    };

    if (config.png2c) {
      console.log("\n── png2c ──────────────────────────");
      for (const e of config.png2c) {
        const { w, h } = await png2c({ inputPath: e.input, outputPath: e.output, symbolName: e.sym, invert: e.invert });
        console.log(`  ${path.basename(e.input)} → ${e.sym} (${w}×${h})`);
      }
    }

    if (config.seqC) {
      console.log("\n── seqC (PNG sequence → C header) ─");
      for (const e of config.seqC) {
        const frames = await pngSeq2c({ inputDir: e.input, outputPath: e.output, baseSymbol: e.baseSym });
        console.log(`  ${path.basename(e.input)} → ${e.baseSym} (${frames.length} frames)`);
      }
    }

    if (config.seq2panim) {
      console.log("\n── seq2panim ───────────────────────");
      for (const e of config.seq2panim)
        await seq2panim({ inputDir: e.input, outputPath: e.output });
    }

    console.log("\nDone.");

  } else {
    console.error(`Unknown command: ${cmd ?? "(none)"}`);
    console.error("Commands: png2c | seq2panim | batch");
    process.exit(1);
  }
}

main().catch(e => { console.error(e); process.exit(1); });
