// palanu add <name> <target> — register a device connection.

import { addDevice } from "../registry.js";
import { NodeWebSocketTransport } from "../transport/NodeWebSocketTransport.js";

export async function addCommand(name: string, target: string): Promise<void> {
	// Auto-add scheme if bare hostname or path given
	let resolved = target;
	if (!target.includes("://")) {
		if (target.startsWith("/")) {
			resolved = `serial://${target}`;       // /dev/cu.usbmodem123 → serial:///dev/...
		} else {
			// Bare hostname → normalize via WebSocket helper (adds port 8477 + /plp)
			resolved = NodeWebSocketTransport.normalizeUrl(target);
		}
	}

	// Validate the resolved URL
	try {
		const url = new URL(resolved);
		if (!["serial:", "ws:", "wss:"].includes(url.protocol)) {
			throw new Error(`unsupported protocol: ${url.protocol}`);
		}
	} catch (e) {
		throw new Error(`invalid target "${target}": ${(e as Error).message}`);
	}

	await addDevice(name, resolved);
	console.log(`✓ added device "${name}" → ${resolved}`);
}
