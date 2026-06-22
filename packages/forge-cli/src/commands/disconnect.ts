// palanu disconnect <name> — disconnect from a device.

import { clearActiveSession, getActiveSession } from "../registry.js";

export async function disconnectCommand(name: string): Promise<void> {
	const session = getActiveSession(name);
	if (!session) {
		console.log(`not connected to "${name}"`);
		return;
	}
	clearActiveSession(name);
	console.log(`✓ disconnected from "${name}"`);
}
