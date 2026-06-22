// palanu remove <name> — remove a device from the registry.

import { removeDevice, clearActiveSession } from "../registry.js";

export async function removeCommand(name: string): Promise<void> {
	// Disconnect if active
	clearActiveSession(name);
	await removeDevice(name);
	console.log(`✓ removed device "${name}"`);
}
