import React, { useEffect } from "react";
import { createRoot } from "react-dom/client";
import { useSimSocket } from "./lib/useSimSocket";
import { LogsPanel }      from "./components/LogsPanel";
import { EventsPanel }    from "./components/EventsPanel";
import { ServicesPanel }  from "./components/ServicesPanel";
import { DisplayPanel }   from "./components/DisplayPanel";
import { HardwareButtons } from "./components/HardwareButtons";
import { SettingsTabs }   from "./components/SettingsTabs";

function App() {
  const { state, send } = useSimSocket();

  const boot         = () => send({ cmd: "boot" });
  const shutdown     = () => send({ cmd: "shutdown" });
  const restart      = () => send({ cmd: "restart" });
  const injectEvent  = (name: string, payload: Record<string, string>) =>
    send({ cmd: "inject_event", name, payload });
  const wifiDisconnect = ()              => send({ cmd: "wifi_disconnect" });
  const pressKey       = (key: string)  => send({ cmd: "press_key", key });
  const setResolution  = (w: number, h: number) => send({ cmd: "set_resolution", w, h });

  // Physical keyboard → 6 hardware buttons (only when sim running)
  useEffect(() => {
    const map: Record<string, string> = {
      ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left", ArrowRight: "Right",
      Enter: "Select", " ": "Select",
      Escape: "Cancel", Backspace: "Cancel",
    };
    const onKey = (e: KeyboardEvent) => {
      const k = map[e.key];
      if (k && state.running) { e.preventDefault(); pressKey(k); }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [state.running]);

  const statusColor = state.running ? "#66bb6a" : state.connected ? "#ffa726" : "#ef5350";
  const statusText  = state.running ? "running"  : state.connected ? "idle"    : "disconnected";

  return (
    <div style={layout.root}>
      {/* Header */}
      <div style={layout.header}>
        <span style={layout.title}>Kairo Simulator</span>
        <span style={{ ...layout.dot, background: statusColor }} />
        <span style={{ color: statusColor, fontSize: 11 }}>{statusText}</span>
        {state.system && (
          <span style={layout.sysInfo}>
            {state.system.platform} / {state.system.board} · v{state.system.buildVersion}
          </span>
        )}
        <span style={layout.res}>{state.resolution.w}×{state.resolution.h}</span>
        {!state.binExists && (
          <span style={layout.warn}>⚠ binary not found — run bun run build:firmware</span>
        )}
      </div>

      {/* Main grid: device | settings | logs */}
      <div style={layout.grid}>
        {/* Device column: display + hardware buttons */}
        <div style={layout.device}>
          <div style={layout.display}>
            <DisplayPanel
              frame={state.frame}
              displaySleeping={state.displaySleeping}
              resolution={state.resolution}
            />
          </div>
          <div style={layout.buttons}>
            <HardwareButtons onKey={pressKey} running={state.running} />
          </div>
        </div>

        {/* Settings column: tabbed WiFi / Display / System */}
        <div style={layout.settings}>
          <SettingsTabs
            running={state.running}
            onBoot={boot} onShutdown={shutdown} onRestart={restart}
            onInjectEvent={injectEvent}
            onWifiDisconnect={wifiDisconnect}
            onSend={(msg) => send(msg)}
            onSetResolution={setResolution}
            wifi={state.wifi}
            resolution={state.resolution}
            system={state.system}
          />
        </div>

        {/* Logs column: services + logs + events */}
        <div style={layout.right}>
          <div style={layout.services}>
            <ServicesPanel services={state.services} />
          </div>
          <div style={layout.logs}>
            <LogsPanel logs={state.logs} />
          </div>
          <div style={layout.events}>
            <EventsPanel events={state.events} />
          </div>
        </div>
      </div>
    </div>
  );
}

const layout = {
  root:     { display: "flex", flexDirection: "column" as const, height: "100vh", overflow: "hidden" },
  header:   { display: "flex", alignItems: "center", gap: 10, padding: "6px 14px", background: "#181818", borderBottom: "1px solid #222", flexShrink: 0, minHeight: 32 },
  title:    { fontWeight: 800, fontSize: 14, color: "#fff", letterSpacing: 0.5 },
  dot:      { width: 8, height: 8, borderRadius: "50%", flexShrink: 0 },
  sysInfo:  { marginLeft: "auto", color: "#555", fontSize: 11 },
  res:      { color: "#4fc3f7", fontSize: 11, fontWeight: 700 },
  warn:     { color: "#ff7043", fontSize: 11 },
  // grid: device (auto, fits display) | settings (340px) | logs (flex)
  grid:     { flex: 1, display: "grid", gridTemplateColumns: "auto 340px 1fr", overflow: "hidden" },
  device:   { display: "flex", flexDirection: "column" as const, borderRight: "1px solid #222", overflow: "auto", background: "#111" },
  display:  { flex: "0 0 auto", overflow: "hidden" },
  buttons:  { flex: "0 0 auto", borderTop: "1px solid #222", background: "#141414" },
  settings: { borderRight: "1px solid #222", overflow: "hidden" },
  right:    { display: "flex", flexDirection: "column" as const, overflow: "hidden" },
  services: { flex: "0 0 160px", overflow: "hidden", borderBottom: "1px solid #222" },
  logs:     { flex: "0 0 55%", overflow: "hidden", borderBottom: "1px solid #222" },
  events:   { flex: 1, overflow: "hidden" },
};

const root = createRoot(document.getElementById("root")!);
root.render(<App />);
