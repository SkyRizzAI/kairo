// Transport factory — creates the right ILinkTransport from a target URL.
// Supported schemes: serial://<port> and ws://<host>:<port>/<path>

import type { ILinkTransport, RemoteSession } from "@palanu/link";
import { RemoteSession as RemoteSessionClass } from "@palanu/link";
import { NodeSerialTransport } from "./transport/NodeSerialTransport.js";
import { NodeWebSocketTransport } from "./transport/NodeWebSocketTransport.js";
import { PreloadedTokenStore } from "./FileTokenStore.js";
import { getDevice, updateDevice } from "./registry.js";

export function createTransport(target: string): ILinkTransport {
	const url = new URL(target);
	switch (url.protocol) {
		case "serial:":
			// serial:///dev/cu.usbmodem123 → path = /dev/cu.usbmodem123
			return new NodeSerialTransport(url.pathname);
		case "ws:":
		case "wss:":
			// ws://host:port/plp → full URL
			return new NodeWebSocketTransport(target);
		default:
			throw new Error(`unsupported transport: ${url.protocol} (use serial:// or ws://)`);
	}
}

export async function createSession(deviceName: string): Promise<RemoteSession> {
	const dev = await getDevice(deviceName);
	const transport = createTransport(dev.target);
	const token = dev.token;
	const tokenStore = new PreloadedTokenStore(deviceName, token);
	const session = new RemoteSessionClass(transport, { tokenStore });

	// Boot the transport (opens serial port / WebSocket connection)
	await transport.boot?.();

	// Wait for handshake (ready event) with a timeout
	await waitForReady(session, 10000);

	// Update lastConnected timestamp
	await updateDevice(deviceName, { lastConnected: new Date().toISOString() });

	return session;
}

export function waitForReady(session: RemoteSession, timeoutMs: number): Promise<void> {
	return new Promise((resolve, reject) => {
		if (session.ready) {
			resolve();
			return;
		}
		const timer = setTimeout(() => {
			reject(new Error(`connection timeout — device did not respond within ${timeoutMs / 1000}s`));
		}, timeoutMs);

		session.on("ready", () => {
			clearTimeout(timer);
			resolve();
		});

		session.on("rejected", () => {
			clearTimeout(timer);
			reject(new Error("device rejected connection — remote is disabled on the device"));
		});
	});
}

// Parse a path like "device:mydev:/path/to/file" → { device: "mydev", path: "/path/to/file" }
// or "./local/path" → { device: null, path: "./local/path" }
export function parsePath(spec: string): { device: string | null; path: string } {
	const m = spec.match(/^device:(\w+):(.+)$/);
	if (m) return { device: m[1], path: m[2] };
	return { device: null, path: spec };
}
