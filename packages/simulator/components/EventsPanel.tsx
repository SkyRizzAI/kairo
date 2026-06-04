import React, { useRef, useEffect } from "react";
import type { EventMsg } from "../lib/useSimSocket";

function fmt(ts: number) {
  return new Date(ts).toTimeString().slice(0, 8);
}

export function EventsPanel({ events }: { events: EventMsg[] }) {
  const bottomRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: "auto" });
  }, [events]);

  return (
    <div style={styles.panel}>
      <div style={styles.header}>Events</div>
      <div style={styles.scroll}>
        {events.map((e, i) => (
          <div key={i} style={styles.row}>
            <span style={styles.ts}>{fmt(e.ts)}</span>
            <span style={styles.name}>{e.name}</span>
            {e.payload && (
              <span style={styles.payload}>
                {Object.entries(e.payload).map(([k, v]) => `${k}=${v}`).join("  ")}
              </span>
            )}
          </div>
        ))}
        <div ref={bottomRef} />
      </div>
    </div>
  );
}

const styles = {
  panel:   { display: "flex", flexDirection: "column" as const, height: "100%", background: "#111", border: "1px solid #222" },
  header:  { padding: "8px 12px", background: "#1a1a1a", borderBottom: "1px solid #222", fontWeight: 700, fontSize: 12, color: "#aaa" },
  scroll:  { flex: 1, overflow: "auto", padding: "4px 0" },
  row:     { display: "flex", gap: 10, padding: "2px 8px", alignItems: "baseline", lineHeight: 1.6, borderBottom: "1px solid #1a1a1a" },
  ts:      { color: "#555", flexShrink: 0, fontSize: 11 },
  name:    { color: "#80cbc4", fontWeight: 700, flexShrink: 0 },
  payload: { color: "#888", fontSize: 11 },
};
