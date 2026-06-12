// Ambient `nema` system API — implemented by the device JS host (Plan 37 Fase 4),
// declared here so custom apps get full TypeScript types. Capability-gated at
// runtime: methods for hardware the board lacks are absent (guard with nema.device).

export interface NemaSystem {
  log(level: "trace" | "debug" | "info" | "warn" | "error", tag: string, msg: string): void;

  device: {
    name: string;
    caps: string[];                 // e.g. ["wifi","bluetooth.ble","display","input.touch"]
    has(cap: string): boolean;
  };

  http?: {
    get(url: string, opts?: { headers?: Record<string, string> }): Promise<HttpResponse>;
    post(url: string, body: string, opts?: { headers?: Record<string, string> }): Promise<HttpResponse>;
  };

  wifi?: {
    scan(): Promise<Array<{ ssid: string; rssi: number }>>;
    connect(ssid: string, password?: string): Promise<boolean>;
    status(): { connected: boolean; ssid?: string };
  };

  ble?: {
    advertise(name: string): void;
    onData(cb: (data: Uint8Array) => void): void;
    send(data: Uint8Array): void;
  };

  storage: {
    get(key: string): string | null;     // per-app namespace
    set(key: string, value: string): void;
    remove(key: string): void;
  };

  timer: {
    setTimeout(cb: () => void, ms: number): number;
    setInterval(cb: () => void, ms: number): number;
    clear(id: number): void;
  };
}

export interface HttpResponse {
  status: number;
  body: string;
  headers: Record<string, string>;
}

declare global {
  // eslint-disable-next-line no-var
  const nema: NemaSystem;
}
