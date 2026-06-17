// Ambient `nema` system API — implemented by the device JS host (Plan 37 Fase 4,
// Plan 48 Fase 4 canonical paths). Declared here so custom apps get full
// TypeScript types. Capability-gated at runtime: methods for hardware the board
// lacks are absent (guard with nema.sys.device or nema.device).
//
// Canonical paths (SSOT = IDL):
//   nema.sys.log.log()            nema.sys.device.has/available
//   nema.storage.kv.get/set       nema.net.http.get
//   nema.profile.*
//
// Deprecated flat aliases (one major release): nema.log(), nema.device.*,
// nema.storage.*, nema.http.* — still work but prefer canonical paths.

export interface NemaSystem {
  // ── Canonical paths ──────────────────────────────────────────────────

  sys: {
    log: {
      log(level: "trace" | "debug" | "info" | "warn" | "error" | "fatal", tag: string, msg: string): void;
    };
    device: {
      name: string;
      caps: string[];                 // e.g. ["net.wifi","bt.ble","display","input.touch"]
      has(cap: string): boolean;      // static: box was built able to do X?
      available(cap: string): boolean; // dynamic: is X up and usable now?
    };
  };

  storage: {
    kv: {
      get(key: string): string | null;     // per-app namespace
      set(key: string, value: string): void;
      remove(key: string): boolean;        // true if key existed
    };
    // deprecated aliases on storage namespace (one major release)
    get(key: string): string | null;
    set(key: string, value: string): void;
    remove(key: string): boolean;
  };

  net: {
    http?: {
      get(url: string): Promise<HttpResponse>;
    };
    wifi?: {
      scan(): Promise<Array<{ ssid: string; rssi: number }>>;
      connect(ssid: string, password?: string): Promise<boolean>;
      status(): { connected: boolean; ssid?: string };
    };
  };

  profile?: {
    userName(): string;
    deviceName(): string;
    hasPassword(): boolean;
    verifyPassword(input: string): boolean;
  };

  // ── Deprecated aliases (Plan 48 Fase 4 — prefer canonical paths above) ──
  log(level: "trace" | "debug" | "info" | "warn" | "error" | "fatal", tag: string, msg: string): void;
  device: NemaSystem["sys"]["device"];
  http?: NemaSystem["net"]["http"];
}

export interface HttpResponse {
  status: number;
  body: string;
}

declare global {
  const nema: NemaSystem;
}
