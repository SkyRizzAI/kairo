// palanu connect <name> — test connection to a device by alias.
// Connects, confirms handshake, then disconnects. Each CLI invocation is a
// separate process, so the in-memory session can't persist — `shell` and `cp`
// auto-connect when needed. This command is just a connectivity check.

import { createSession } from "../session.js";

export async function connectCommand(name: string, password?: string): Promise<void> {
	console.log(`connecting to "${name}"…`);
	try {
		const session = await createSession(name, { password });
		console.log(`✓ connected to "${name}" (${session.profile?.name ?? "unknown board"})`);
		session.close();
	} catch (e) {
		throw new Error(`failed to connect: ${(e as Error).message}`);
	}
}
