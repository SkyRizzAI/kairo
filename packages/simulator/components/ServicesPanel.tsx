import React from "react";

const STATE_COLOR: Record<string, string> = {
  Running:  "#66bb6a",
  Stopped:  "#555",
  Failed:   "#ef5350",
  Starting: "#ffa726",
  Stopping: "#ffa726",
  Created:  "#777",
};

export function ServicesPanel({ services }: { services: Record<string, string> }) {
  const entries = Object.entries(services);

  return (
    <div style={styles.panel}>
      <div style={styles.header}>Services</div>
      <div style={styles.scroll}>
        {entries.length === 0
          ? <div style={styles.empty}>No services running</div>
          : entries.map(([name, state]) => (
            <div key={name} style={styles.row}>
              <span style={{ ...styles.dot, background: STATE_COLOR[state] ?? "#777" }} />
              <span style={styles.name}>{name}</span>
              <span style={{ ...styles.state, color: STATE_COLOR[state] ?? "#aaa" }}>{state}</span>
            </div>
          ))}
      </div>
    </div>
  );
}

const styles = {
  panel:  { display: "flex", flexDirection: "column" as const, height: "100%", background: "#111", border: "1px solid #222" },
  header: { padding: "8px 12px", background: "#1a1a1a", borderBottom: "1px solid #222", fontWeight: 700, fontSize: 12, color: "#aaa" },
  scroll: { flex: 1, overflow: "auto", padding: "6px 0" },
  empty:  { padding: "12px", color: "#444" },
  row:    { display: "flex", alignItems: "center", gap: 8, padding: "5px 12px", borderBottom: "1px solid #1a1a1a" },
  dot:    { width: 8, height: 8, borderRadius: "50%", flexShrink: 0 },
  name:   { flex: 1, color: "#ddd" },
  state:  { fontSize: 11, fontWeight: 700 },
};
