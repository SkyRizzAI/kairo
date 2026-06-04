import { resolve, dirname } from "path";
import { existsSync } from "fs";
import index from "./index.html";

const REPO_ROOT = resolve(dirname(Bun.main), "..", "..");
const BIN_PATH  = resolve(REPO_ROOT, "firmware/build/targets/simulator/kairo-sim");
const RESTART_EXIT_CODE = 75;

// ---------------------------------------------------------------------------
// Spawn manager
// ---------------------------------------------------------------------------
let proc: ReturnType<typeof Bun.spawn> | null = null;
let serverRef: ReturnType<typeof Bun.serve> | null = null;
let simResolution = { w: 264, h: 176 };

function broadcast(obj: unknown) {
  serverRef?.publish("sim", JSON.stringify(obj));
}

async function pumpStdout(p: ReturnType<typeof Bun.spawn>) {
  if (!p.stdout) return;
  const reader = (p.stdout as ReadableStream<Uint8Array>).getReader();
  const dec = new TextDecoder();
  let buf = "";
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    buf += dec.decode(value);
    let nl: number;
    while ((nl = buf.indexOf("\n")) >= 0) {
      const line = buf.slice(0, nl).trim();
      buf = buf.slice(nl + 1);
      if (!line) continue;
      try { broadcast(JSON.parse(line)); }
      catch { /* skip non-JSON lines */ }
    }
  }
}

function bootSim() {
  if (proc) return;
  if (!existsSync(BIN_PATH)) {
    broadcast({ type: "error", message: `Binary not found. Run: bun run build:firmware` });
    return;
  }
  proc = Bun.spawn([BIN_PATH], {
    env: {
      ...process.env,
      KAIRO_SIM_JSON: "1",
      KAIRO_SIM_W: String(simResolution.w),
      KAIRO_SIM_H: String(simResolution.h),
    },
    stdin:  "pipe",
    stdout: "pipe",
    stderr: "inherit",
    onExit(_p, code) {
      proc = null;
      broadcast({ type: "sim_exit", code });
      if (code === RESTART_EXIT_CODE) bootSim();
    },
  });
  pumpStdout(proc);
}

function sendToSim(msg: unknown) {
  if (!proc?.stdin) return;
  proc.stdin.write(JSON.stringify(msg) + "\n");
}

// ---------------------------------------------------------------------------
// Bun server
// ---------------------------------------------------------------------------
const server = Bun.serve({
  routes: { "/": index },

  fetch(req, srv) {
    if (new URL(req.url).pathname === "/ws" && srv.upgrade(req)) return;
    return new Response("Kairo Simulator");
  },

  websocket: {
    open(ws) {
      ws.subscribe("sim");
      ws.send(JSON.stringify({ type: "hello", binExists: existsSync(BIN_PATH) }));
    },
    message(_ws, raw) {
      const msg = JSON.parse(String(raw)) as Record<string, unknown>;
      const cmd = msg.cmd as string | undefined;
      if (cmd === "boot")          bootSim();
      else if (cmd === "shutdown") sendToSim({ cmd: "shutdown" });
      else if (cmd === "restart")  sendToSim({ cmd: "restart" });
      else if (cmd === "set_resolution") {
        // Persist new resolution and (re)boot the sim with it.
        const w = Number(msg.w) || 264;
        const h = Number(msg.h) || 176;
        simResolution = { w, h };
        if (proc) {
          sendToSim({ cmd: "shutdown" });
          setTimeout(bootSim, 250);   // let it exit, then relaunch at new res
        } else {
          bootSim();
        }
        broadcast({ type: "resolution", w, h });
      }
      else                         sendToSim(msg);
    },
    close(ws) { ws.unsubscribe("sim"); },
  },

  development: { hmr: true, console: true },
});

serverRef = server;
console.log(`Kairo Simulator  →  http://localhost:${server.port}`);
console.log(`Binary           →  ${BIN_PATH}`);
