import React, { useRef, useEffect, useState } from "react";
import type { LogMsg } from "../lib/useSimSocket";

const LEVEL_COLOR: Record<string, string> = {
  TRACE: "#666", DEBUG: "#888", INFO: "#4fc3f7",
  WARN: "#ffb74d", ERROR: "#ef5350", FATAL: "#e040fb",
};

const ALL_LEVELS = ["TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"];

function fmt(ts: number) {
  const d = new Date(ts);
  return d.toTimeString().slice(0, 8);
}

export function LogsPanel({ logs }: { logs: LogMsg[] }) {
  const [filter, setFilter] = useState<Set<string>>(new Set(ALL_LEVELS));
  const bottomRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: "auto" });
  }, [logs]);

  const visible = logs.filter(l => filter.has(l.level));

  return (
    <div style={styles.panel}>
      <div style={styles.header}>
        Logs
        <span style={{ marginLeft: 12, display: "flex", gap: 6 }}>
          {ALL_LEVELS.map(lvl => (
            <button
              key={lvl}
              onClick={() => setFilter(f => {
                const next = new Set(f);
                next.has(lvl) ? next.delete(lvl) : next.add(lvl);
                return next;
              })}
              style={{ ...styles.chip, opacity: filter.has(lvl) ? 1 : 0.3, color: LEVEL_COLOR[lvl] ?? "#fff", borderColor: LEVEL_COLOR[lvl] ?? "#555" }}
            >
              {lvl}
            </button>
          ))}
        </span>
      </div>
      <div style={styles.scroll}>
        {visible.map((l, i) => (
          <div key={i} style={{ ...styles.logRow, color: LEVEL_COLOR[l.level] ?? "#ccc" }}>
            <span style={styles.ts}>{fmt(l.ts)}</span>
            <span style={{ ...styles.badge, color: LEVEL_COLOR[l.level] ?? "#ccc" }}>{l.level.padEnd(5)}</span>
            <span style={styles.comp}>[{l.component}]</span>
            <span style={styles.msg}>{l.message}</span>
            {l.fields && Object.entries(l.fields).map(([k, v]) => (
              <span key={k} style={styles.field}>{k}={v}</span>
            ))}
          </div>
        ))}
        <div ref={bottomRef} />
      </div>
    </div>
  );
}

const styles = {
  panel:  { display: "flex", flexDirection: "column" as const, height: "100%", background: "#111", border: "1px solid #222" },
  header: { padding: "8px 12px", background: "#1a1a1a", borderBottom: "1px solid #222", fontWeight: 700, fontSize: 12, color: "#aaa", display: "flex", alignItems: "center" },
  scroll: { flex: 1, overflow: "auto", padding: "4px 0" },
  logRow: { display: "flex", alignItems: "baseline", gap: 6, padding: "1px 8px", lineHeight: 1.6 },
  ts:     { color: "#555", flexShrink: 0, fontSize: 11 },
  badge:  { flexShrink: 0, fontWeight: 700, fontSize: 11, fontFamily: "monospace" },
  comp:   { color: "#7986cb", flexShrink: 0, fontSize: 11 },
  msg:    { color: "#ddd", flex: 1 },
  field:  { color: "#888", fontSize: 11, marginLeft: 4 },
  chip:   { background: "transparent", border: "1px solid", borderRadius: 3, padding: "1px 5px", cursor: "pointer", fontSize: 10, fontWeight: 700 },
};
