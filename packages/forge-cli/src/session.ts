// Transport factory — creates the right ILinkTransport from a target URL.
// Supported schemes: serial://<port> and ws://<host>:<port>/<path>

import type { ILinkTransport, RemoteSession } from "@palanu/link";
import { RemoteSession as RemoteSessionClass } from "@palanu/link";
import { NodeSerialTransport } from "./transport/NodeSerialTransport.js";
import { NodeWebSocketTransport } from "./transport/NodeWebSocketTransport.js";
import { PreloadedTokenStore } from "./FileTokenStore.js";
import { getDevice, updateDevice } from "./registry.js";

// Bun lacks libuv (uv_default_loop), so 'serialport' npm crashes on connect.
// Use a bun:ffi-based transport instead; Node.js keeps NodeSerialTransport.
const isBun = typeof process.versions.bun !== "undefined";

export async function createTransport(target: string): Promise<ILinkTransport> {
	const url = new URL(target);
	switch (url.protocol) {
		case "serial:": {
			if (isBun) {
				const { BunSerialTransport } = await import("./transport/BunSerialTransport.js");
				return new BunSerialTransport(url.pathname);
			}
			return new NodeSerialTransport(url.pathname);
		}
		case "ws:":
		case "wss:":
			return new NodeWebSocketTransport(target);
		default:
			throw new Error(`unsupported transport: ${url.protocol} (use serial:// or ws://)`);
	}
}

export interface CreateSessionOptions {
	password?: string; // fall back to challenge-response if the stored token is rejected
}

export async function createSession(
	deviceName: string,
	opts: CreateSessionOptions = {}
): Promise<RemoteSession> {
	const dev = await getDevice(deviceName);
	const transport = await createTransport(dev.target);
	const token = dev.token;
	const tokenStore = new PreloadedTokenStore(deviceName, token);
	const session = new RemoteSessionClass(transport, { tokenStore });

	// Boot the transport (opens serial port / WebSocket connection)
	await transport.boot?.();

	// 1) Wait for PLP handshake (ACK received)
	await waitForReady(session, 10000);

	// 2) Authenticate. File/system ops are only safe once authorized. This no longer
	//    proceeds silently when the device is locked (Plan 88 S4).
	await authenticate(session, opts.password);

	// 3) Give the board profile (GetInfo, requested post-auth) a moment to arrive so
	//    callers see the real board name, not "unknown board". Optional — old firmware
	//    may never answer, so we just time out and continue.
	await waitForProfile(session, 1500);

	// Update lastConnected timestamp
	await updateDevice(deviceName, { lastConnected: new Date().toISOString() });

	return session;
}

export function waitForReady(session: RemoteSession, timeoutMs: number): Promise<void> {
	return new Promise((resolve, reject) => {
		if (session.ready) { resolve(); return; }
		const timer = setTimeout(
			() => reject(new Error(`connection timeout — device did not respond within ${timeoutMs / 1000}s`)),
			timeoutMs
		);
		session.on("ready",    () => { clearTimeout(timer); resolve(); });
		session.on("rejected", () => {
			clearTimeout(timer);
			reject(new Error("device rejected connection — Remote is disabled in device Settings"));
		});
	});
}

type AuthOutcome = "authorized" | "fail" | "open" | "required";

// Wait for auth to settle one way or another. Distinguishes a genuinely open device
// (no challenge → "open") from a locked one we couldn't satisfy ("required"), instead
// of silently proceeding on timeout (the Plan 88 S4 bug that let the CLI run File ops
// against a locked device and get every request dropped).
function settleAuth(session: RemoteSession, timeoutMs: number): Promise<AuthOutcome> {
	return new Promise((resolve) => {
		if (session.authorized) { resolve("authorized"); return; }
		const offs: Array<() => void> = [];
		const done = (v: AuthOutcome) => {
			clearTimeout(timer);
			for (const o of offs) o();
			resolve(v);
		};
		const timer = setTimeout(() => done(session.authRequired ? "required" : "open"), timeoutMs);
		offs.push(session.on("authorized", () => done("authorized")));
		offs.push(session.on("authfail", () => done("fail")));
	});
}

// Drive authentication, falling back to a password (challenge-response) when the
// stored token is missing/rejected. Throws a clear error if the device stays locked.
export async function authenticate(session: RemoteSession, password?: string): Promise<void> {
	const first = await settleAuth(session, 6000);
	if (first === "authorized" || first === "open") return;

	// Token rejected ("fail") or timed out while the device wanted auth ("required").
	if (password) {
		await session.submitPassword(password);
		const second = await settleAuth(session, 8000);
		if (second === "authorized") return;
		throw new Error("authentication failed — wrong password.");
	}
	throw new Error(
		"device is password-protected and the stored token was rejected.\n" +
		"  Re-run with the password:  …  --password <pw>   (it's saved as a fresh token on success)."
	);
}

// Back-compat shim: old callers used waitForAuthorized; route to the robust path.
export async function waitForAuthorized(session: RemoteSession, _timeoutMs = 6000): Promise<void> {
	await authenticate(session);
}

// Wait up to timeoutMs for the board profile (GetInfo reply). Resolves either way —
// the profile is informational, so a device that never answers shouldn't block.
function waitForProfile(session: RemoteSession, timeoutMs: number): Promise<void> {
	return new Promise((resolve) => {
		if (session.profile) { resolve(); return; }
		const off = session.on('profile', () => { off(); clearTimeout(t); resolve(); });
		const t = setTimeout(() => { off(); resolve(); }, timeoutMs);
	});
}

// Parse a path like "device:mydev:/path/to/file" → { device: "mydev", path: "/path/to/file" }
// or "./local/path" → { device: null, path: "./local/path" }
export function parsePath(spec: string): { device: string | null; path: string } {
	const m = spec.match(/^device:(\w+):(.+)$/);
	if (m) return { device: m[1], path: m[2] };
	return { device: null, path: spec };
}
