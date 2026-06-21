// Parse Flipper Zero animation meta.txt format.

export interface FlipperMeta {
  width: number;
  height: number;
  passiveFrames: number;
  activeFrames: number;
  framesOrder: number[];
  activeCycles: number;
  frameRate: number;
  duration: number;
  activeCooldown: number;
}

export function parseMeta(text: string): FlipperMeta {
  const get = (key: string): string => {
    const re = new RegExp(`^${key}:\\s*(.+)$`, "m");
    const m = text.match(re);
    if (!m) throw new Error(`meta.txt: missing key "${key}"`);
    return m[1].trim();
  };

  return {
    width:          parseInt(get("Width")),
    height:         parseInt(get("Height")),
    passiveFrames:  parseInt(get("Passive frames")),
    activeFrames:   parseInt(get("Active frames")),
    framesOrder:    get("Frames order").split(/\s+/).map(Number),
    activeCycles:   parseInt(get("Active cycles")),
    frameRate:      parseInt(get("Frame rate")),
    duration:       parseInt(get("Duration")),
    activeCooldown: parseInt(get("Active cooldown")),
  };
}
