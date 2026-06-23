// palanu ls <name> [path] — list files/directories on a device.
// Like `ls -la` over the remote File channel.

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

export async function lsCommand(spec: string, password?: string): Promise<void> {
	const parsed = parsePath(spec);
	if (!parsed.device) throw new Error("specify a device path: device:<name>:/path");

	const session = await getSession(parsed.device, password);
	const entries = await session.listDir(parsed.path);
	if (!entries) {
		throw new Error(
			`cannot list ${parsed.path} on "${parsed.device}" — ` +
			(parsed.path.startsWith("/sd")
				? "SD card not mounted or path does not exist"
				: "path does not exist or device error")
		);
	}

	if (entries.length === 0) {
		console.log("(empty directory)");
		return;
	}

	for (const e of entries) {
		const sizeCol = e.isDir ? "       -" : `${e.size}B`.padStart(8);
		console.log(`${e.isDir ? "d" : "-"}  ${sizeCol}  ${e.name}`);
	}
}
