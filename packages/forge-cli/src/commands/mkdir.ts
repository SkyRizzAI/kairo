// palanu mkdir <name>:<path> — create a directory on a device.

import { getActiveSession, setActiveSession } from "../registry.js";
import { createSession, parsePath } from "../session.js";

async function getSession(name: string, password?: string) {
	let session = getActiveSession(name);
	if (!session || !session.ready) {
		session = await createSession(name, { password });
		setActiveSession(name, session);
	}
	return session;
}

export async function mkdirCommand(spec: string, password?: string): Promise<void> {
	const parsed = parsePath(spec);
	if (!parsed.device) throw new Error("specify a device path: device:<name>:/path");

	const session = await getSession(parsed.device, password);
	const ok = await session.mkdir(parsed.path);
	if (!ok) {
		throw new Error(
			`failed to create ${parsed.path} on "${parsed.device}" — ` +
			(parsed.path.startsWith("/sd")
				? "SD card not mounted, path already exists, or device error"
				: "device error")
		);
	}
	console.log(`✓ created ${parsed.device}:${parsed.path}`);
}
