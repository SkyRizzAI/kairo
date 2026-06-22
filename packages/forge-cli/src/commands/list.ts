// palanu list — list all registered devices.
// Shows last connection time from the persistent registry (each CLI command
// is a separate process, so in-memory session state doesn't persist between
// commands — `list` reflects the registry, not a live connection).

import { listDevices } from "../registry.js";

export async function listCommand(): Promise<void> {
	const devices = await listDevices();
	const names = Object.keys(devices);

	if (names.length === 0) {
		console.log("No devices registered. Run: palanu add <name> <target>");
		return;
	}

	// Calculate column widths
	const nameWidth = Math.max(4, ...names.map((n) => n.length));
	const targetWidth = Math.max(6, ...names.map((n) => devices[n].target.length));

	console.log(`${"NAME".padEnd(nameWidth)}  ${"TARGET".padEnd(targetWidth)}  LAST SEEN`);
	console.log(`${"─".repeat(nameWidth)}  ${"─".repeat(targetWidth)}  ─────────`);

	for (const name of names.sort()) {
		const dev = devices[name];
		const lastSeen = dev.lastConnected
			? new Date(dev.lastConnected).toLocaleString()
			: "never";
		console.log(`${name.padEnd(nameWidth)}  ${dev.target.padEnd(targetWidth)}  ${lastSeen}`);
	}
}
