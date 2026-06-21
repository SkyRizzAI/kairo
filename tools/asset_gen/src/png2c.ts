// Convert a single PNG to a C array header (T1 icon / T2 anim frame).
// Bit convention: 1 = pixel ON (dark/black), 0 = pixel OFF (white/light).
// Packing: MSB first, row-major — matches Canvas::drawBitmap().

import { Jimp } from "jimp";
import * as path from "path";
import * as fs from "fs";

export interface Png2cOptions {
  inputPath: string;
  outputPath: string;
  symbolName: string;   // C symbol, e.g. "kIcBattery"
  threshold?: number;   // 0-255, default 128; pixel < threshold → ON
  appendMode?: boolean; // append to existing file instead of overwriting
}

export async function png2c(opts: Png2cOptions): Promise<{ w: number; h: number }> {
  const { inputPath, outputPath, symbolName, threshold = 128, appendMode = false } = opts;

  const img = await Jimp.read(inputPath);
  const w = img.bitmap.width;
  const h = img.bitmap.height;

  // Convert to grayscale + threshold → 1-bit row-major MSB-first
  const bytesPerRow = Math.ceil(w / 8);
  const bytes = new Uint8Array(bytesPerRow * h);

  for (let row = 0; row < h; row++) {
    for (let col = 0; col < w; col++) {
      const pixel = img.getPixelColor(col, row);
      // RGBA packed int: 0xRRGGBBAA
      const r = (pixel >>> 24) & 0xff;
      const g = (pixel >>> 16) & 0xff;
      const b = (pixel >>> 8)  & 0xff;
      const gray = (r * 299 + g * 587 + b * 114) / 1000;
      if (gray < threshold) {
        // pixel ON → set bit (MSB first within byte)
        const bitIdx = row * w + col;
        bytes[bitIdx >> 3] |= (1 << (7 - (bitIdx & 7)));
      }
    }
  }

  // Format as C array
  const hexBytes = Array.from(bytes).map(b => `0x${b.toString(16).padStart(2, "0")}`);
  const rows: string[] = [];
  for (let i = 0; i < hexBytes.length; i += 12)
    rows.push("    " + hexBytes.slice(i, i + 12).join(", "));

  const out = [
    `// ${path.basename(inputPath)} — ${w}×${h} px`,
    `static const uint8_t ${symbolName}[] = {`,
    rows.join(",\n"),
    `};`,
    ``,
  ].join("\n");

  if (appendMode) {
    fs.appendFileSync(outputPath, out);
  } else {
    fs.mkdirSync(path.dirname(outputPath), { recursive: true });
    fs.writeFileSync(outputPath, out);
  }

  return { w, h };
}

// Convert a directory of frame_N.png files to an array of C symbols.
// Returns array of { sym, w, h } in frame order.
export async function pngSeq2c(opts: {
  inputDir: string;
  outputPath: string;
  baseSymbol: string;  // e.g. "kAnimIconSettings" → kAnimIconSettingsF0, F1, …
  threshold?: number;
  appendMode?: boolean;
}): Promise<Array<{ sym: string; w: number; h: number }>> {
  const { inputDir, outputPath, baseSymbol, threshold, appendMode } = opts;

  const pngFiles = fs.readdirSync(inputDir)
    .filter(f => /^frame_\d+\.png$/i.test(f))
    .sort((a, b) => {
      const na = parseInt(a.match(/\d+/)![0]);
      const nb = parseInt(b.match(/\d+/)![0]);
      return na - nb;
    });

  if (pngFiles.length === 0)
    throw new Error(`No frame_N.png files found in ${inputDir}`);

  const results: Array<{ sym: string; w: number; h: number }> = [];
  for (let i = 0; i < pngFiles.length; i++) {
    const sym = `${baseSymbol}F${i}`;
    const { w, h } = await png2c({
      inputPath: path.join(inputDir, pngFiles[i]),
      outputPath,
      symbolName: sym,
      threshold,
      appendMode: appendMode || i > 0,
    });
    results.push({ sym, w, h });
  }
  return results;
}
