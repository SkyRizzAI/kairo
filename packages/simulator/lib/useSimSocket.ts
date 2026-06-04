import { useState, useEffect, useRef, useCallback } from "react";

export interface LogMsg {
  ts: number;
  level: string;
  component: string;
  message: string;
  fields?: Record<string, string>;
}

export interface EventMsg {
  ts: number;
  name: string;
  payload?: Record<string, string>;
}

export interface ServiceEntry {
  name: string;
  state: string;
}

export interface SystemInfo {
  platform: string;
  board: string;
  buildVersion: string;
  firmwareVersion: string;
}

export interface FrameMsg {
  width: number;
  height: number;
  data: string;  // base64 encoded buffer
}

export interface SimState {
  connected: boolean;
  running: boolean;
  binExists: boolean;
  logs: LogMsg[];
  events: EventMsg[];
  services: Record<string, string>;
  system: SystemInfo | null;
  frame: FrameMsg | null;
  wifi: { connected: boolean; ssid: string; ip: string };
  displaySleeping: boolean;
  resolution: { w: number; h: number };
  error: string | null;
}

const MAX_ENTRIES = 500;

function processMsg(prev: SimState, msg: Record<string, unknown>): SimState {
  const type = msg["type"] as string;

  if (type === "hello") {
    return { ...prev, binExists: Boolean(msg["binExists"]) };
  }
  if (type === "log") {
    const entry: LogMsg = {
      ts: msg["ts"] as number,
      level: msg["level"] as string,
      component: msg["component"] as string,
      message: msg["message"] as string,
      fields: msg["fields"] as Record<string, string> | undefined,
    };
    const logs = [...prev.logs, entry].slice(-MAX_ENTRIES);
    return { ...prev, logs };
  }
  if (type === "event") {
    const entry: EventMsg = {
      ts: msg["ts"] as number,
      name: msg["name"] as string,
      payload: msg["payload"] as Record<string, string> | undefined,
    };
    let wifi = prev.wifi;
    if (entry.name === "NetworkConnected")
      wifi = { ...wifi, connected: true, ssid: entry.payload?.ssid ?? "" };
    else if (entry.name === "NetworkDisconnected")
      wifi = { ...wifi, connected: false, ssid: "" };
    const events = [...prev.events, entry].slice(-MAX_ENTRIES);
    if (entry.name === "SystemReady") return { ...prev, running: true, events, wifi };
    return { ...prev, events, wifi };
  }
  if (type === "service") {
    return {
      ...prev,
      services: { ...prev.services, [msg["name"] as string]: msg["state"] as string },
    };
  }
  if (type === "display_sleep") return { ...prev, displaySleeping: true };
  if (type === "display_wake")  return { ...prev, displaySleeping: false };
  if (type === "resolution") {
    return { ...prev, resolution: { w: msg["w"] as number, h: msg["h"] as number } };
  }
  if (type === "frame") {
    const frame: FrameMsg = {
      width:  msg["width"] as number,
      height: msg["height"] as number,
      data:   msg["data"] as string,
    };
    return { ...prev, frame, resolution: { w: frame.width, h: frame.height } };
  }
  if (type === "system") {
    return { ...prev, system: (msg["info"] as SystemInfo) ?? null };
  }
  if (type === "ready") {
    return { ...prev, running: true };
  }
  if (type === "sim_exit") {
    return { ...prev, running: false, services: {} };
  }
  if (type === "error") {
    return { ...prev, error: msg["message"] as string };
  }
  return prev;
}

const INITIAL: SimState = {
  connected: false, running: false, binExists: false,
  logs: [], events: [], services: {}, system: null, frame: null,
  wifi: { connected: false, ssid: "", ip: "0.0.0.0" },
  displaySleeping: false,
  resolution: { w: 264, h: 176 },
  error: null,
};

export function useSimSocket() {
  const [state, setState] = useState<SimState>(INITIAL);
  const wsRef = useRef<WebSocket | null>(null);

  const send = useCallback((msg: unknown) => {
    wsRef.current?.send(JSON.stringify(msg));
  }, []);

  useEffect(() => {
    const ws = new WebSocket(`ws://${window.location.host}/ws`);
    wsRef.current = ws;
    ws.onopen  = () => setState(p => ({ ...p, connected: true }));
    ws.onclose = () => setState(p => ({ ...p, connected: false, running: false }));
    ws.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data as string) as Record<string, unknown>;
        setState(p => processMsg(p, msg));
      } catch { /* ignore bad JSON */ }
    };
    return () => ws.close();
  }, []);

  return { state, send };
}
