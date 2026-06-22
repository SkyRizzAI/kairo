// palanu cp <src> <dst> — copy file between device and local (like scp).
// Path format: device:<name>:/path/to/file (remote) or ./local/path (local).
//
// Examples:
//   palanu cp device:mydev:/system/apps/app.papp ./backup/        # pull
//   palanu cp ./myapp.papp.zip device:mydev:/system/apps/         # push

import { readFile, writeFile, mkdir } from "node:fs/promises";
import { dirname, basename, join, isAbsolute } from "node:path";
import { existsSync, statSync } from "node:fs";
import { getActiveSession, setActiveSession } from "../registry.js";
import { createSession, parsePath } from "../session.js";

export async function cpCommand(src: string, dst: string): Promise<void> {
	const srcParsed = parsePath(src);
	const dstParsed = parsePath(dst);

	// Determine direction: pull (remote→local) or push (local→remote)
	if (srcParsed.device && !dstParsed.device) {
		await pullFile(srcParsed.device, srcParsed.path, dstParsed.path);
	} else if (!srcParsed.device && dstParsed.device) {
		await pushFile(srcParsed.path, dstParsed.device, dstParsed.path);
	} else if (srcParsed.device && dstParsed.device) {
		// remote→remote — not supported in MVP
		throw new Error("remote-to-remote copy not supported. Pull to local first, then push.");
	} else {
		// local→local — not our job
		throw new Error("both paths are local. Use `cp` instead.");
	}
}

async function getSession(name: string) {
	let session = getActiveSession(name);
	if (!session || !session.ready) {
		session = await createSession(name);
		setActiveSession(name, session);
	}
	return session;
}

async function pullFile(deviceName: string, remotePath: string, localPath: string): Promise<void> {
	console.log(`pulling ${deviceName}:${remotePath} → ${localPath}…`);
	const session = await getSession(deviceName);
	const data = await session.readFile(remotePath);
	if (!data) {
		throw new Error(`failed to read ${remotePath} — file not found or device error`);
	}

	// If localPath is a directory, append the remote filename
	let targetPath = localPath;
	if (existsSync(localPath) && statSync(localPath).isDirectory()) {
		targetPath = join(localPath, basename(remotePath));
	}

	await mkdir(dirname(targetPath), { recursive: true });
	await writeFile(targetPath, data);
	console.log(`✓ ${data.length} bytes → ${targetPath}`);
}

async function pushFile(localPath: string, deviceName: string, remotePath: string): Promise<void> {
	console.log(`pushing ${localPath} → ${deviceName}:${remotePath}…`);
	const session = await getSession(deviceName);

	if (!existsSync(localPath)) {
		throw new Error(`local file not found: ${localPath}`);
	}
	const data = await readFile(localPath);

	// If remotePath ends with /, append the local filename
	let targetPath = remotePath;
	if (remotePath.endsWith("/")) {
		targetPath = remotePath + basename(localPath);
	}

	const ok = await session.writeFile(targetPath, new Uint8Array(data));
	if (!ok) {
		throw new Error(`failed to write ${targetPath} — device error or permission denied`);
	}
	console.log(`✓ ${data.length} bytes → ${deviceName}:${targetPath}`);
}
