// Convert a Flipper Zero animation directory (frame_N.png + meta.txt) to .panim.
//
// .panim format (version 1, little-endian):
//   [4]  magic            "PANM"
//   [1]  version          1
//   [2]  width
//   [2]  height
//   [1]  frameRate
//   [1]  passiveCount     playback positions in passive loop
//   [1]  activeCount      playback positions in active sequence
//   [1]  uniqueFrameCount unique bitmaps stored (actual unique PNG count)
//   [1]  framesOrderLen   total entries in framesOrder (passiveCount+activeCount)
//   [N]  framesOrder[]    (uint8, N = framesOrderLen; indices into unique bitmaps)
//   [M]  raw frame bitmaps (ceil(w/8)*h bytes each, uniqueFrameCount total)

import { Jimp } from "jimp";
import { parseMeta } from "./meta_parser.js";
import * as path from "path";
import * as fs from "fs";

export interface Seq2PanimOptions {
  inputDir:   string;  // directory with frame_N.png + meta.txt
  outputPath: string;  // .panim output file
  threshold?: number;  // grayscale threshold, default 128
}

async function pngTo1bit(filePath: string, w: number, h: number, threshold: number): Promise<Uint8Array> {
  const img = await Jimp.read(filePath);
  // Resize to declared size if needed (shouldn't be necessary for Flipper frames)
  if (img.bitmap.width !== w || img.bitmap.height !== h)
    img.resize({ w, h });

  const bytesPerRow = Math.ceil(w / 8);
  const bytes = new Uint8Array(bytesPerRow * h);
  for (let row = 0; row < h; row++) {
    for (let col = 0; col < w; col++) {
      const pixel = img.getPixelColor(col, row);
      const r = (pixel >>> 24) & 0xff;
      const g = (pixel >>> 16) & 0xff;
      const b = (pixel >>> 8)  & 0xff;
      const gray = (r * 299 + g * 587 + b * 114) / 1000;
      if (gray < threshold) {
        const bitIdx = row * w + col;
        bytes[bitIdx >> 3] |= (1 << (7 - (bitIdx & 7)));
      }
    }
  }
  return bytes;
}

export async function seq2panim(opts: Seq2PanimOptions): Promise<void> {
  const { inputDir, outputPath, threshold = 128 } = opts;

  // Read meta
  const metaPath = path.join(inputDir, "meta.txt");
  if (!fs.existsSync(metaPath))
    throw new Error(`meta.txt not found in ${inputDir}`);
  const meta = parseMeta(fs.readFileSync(metaPath, "utf8"));

  // Count unique bitmaps from actual frame files in directory
  const allFiles = fs.readdirSync(inputDir);
  const frameFiles = allFiles
    .filter(f => /^frame_\d+\.png$/i.test(f))
    .sort((a, b) => {
      const na = parseInt(a.match(/\d+/)![0]);
      const nb = parseInt(b.match(/\d+/)![0]);
      return na - nb;
    });
  const uniqueFrames = frameFiles.length;

  // Read and convert all unique frame bitmaps
  const frameBitmaps: Uint8Array[] = [];
  for (const fileName of frameFiles)
    frameBitmaps.push(await pngTo1bit(path.join(inputDir, fileName), meta.width, meta.height, threshold));

  // Build framesOrder table
  const framesOrder = meta.framesOrder.map(v => Math.min(255, Math.max(0, v)));
  const framesOrderLen = framesOrder.length <= 255 ? framesOrder.length : 0;

  // Build binary buffer — fixed header is 14 bytes
  const bytesPerFrame = Math.ceil(meta.width / 8) * meta.height;
  const totalSize = 4 + 1 + 2 + 2 + 1 + 1 + 1 + 1 + 1 + framesOrderLen + uniqueFrames * bytesPerFrame;
  const buf = Buffer.alloc(totalSize);
  let off = 0;

  // Magic
  buf[off++] = 0x50; // P
  buf[off++] = 0x41; // A
  buf[off++] = 0x4E; // N
  buf[off++] = 0x4D; // M
  // Version
  buf[off++] = 1;
  // Width / Height (little-endian uint16)
  buf.writeUInt16LE(meta.width,  off); off += 2;
  buf.writeUInt16LE(meta.height, off); off += 2;
  // Timing / structure
  buf[off++] = Math.min(255, meta.frameRate);
  buf[off++] = Math.min(255, meta.passiveFrames);
  buf[off++] = Math.min(255, meta.activeFrames);
  buf[off++] = Math.min(255, uniqueFrames);
  buf[off++] = framesOrderLen;
  // Frames order
  for (let i = 0; i < framesOrderLen; i++)
    buf[off++] = framesOrder[i];
  // Raw frame data
  for (const bitmap of frameBitmaps) {
    buf.set(bitmap, off);
    off += bitmap.length;
  }

  fs.mkdirSync(path.dirname(outputPath), { recursive: true });
  fs.writeFileSync(outputPath, buf);

  console.log(`  → ${path.basename(outputPath)}  (${meta.width}×${meta.height}, ${uniqueFrames} frames, ${totalSize} bytes)`);
}
