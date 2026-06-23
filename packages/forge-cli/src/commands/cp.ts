// palanu cp <src> <dst> — copy file between device and local (like scp).
// Path format: device:<name>:/path/to/file (remote) or ./local/path (local).
//
// Examples:
//   palanu cp device:mydev:/system/apps/app.papp ./backup/        # pull
//   palanu cp ./myapp.papp.zip device:mydev:/system/apps/         # push

import { readFile, writeFile, mkdir, readdir } from "node:fs/promises";
import { dirname, basename, join, isAbsolute } from "node:path";
import { existsSync, statSync } from "node:fs";
import { unzipSync } from "fflate";
import type { RemoteSession } from "@palanu/link";
import { getActiveSession, setActiveSession } from "../registry.js";
import { createSession, parsePath } from "../session.js";

type Entry = { rel: string; data: Uint8Array };

export async function cpCommand(src: string, dst: string, password?: string): Promise<void> {
	const srcParsed = parsePath(src);
	const dstParsed = parsePath(dst);

	// Determine direction: pull (remote→local) or push (local→remote)
	if (srcParsed.device && !dstParsed.device) {
		await pullFile(srcParsed.device, srcParsed.path, dstParsed.path, password);
	} else if (!srcParsed.device && dstParsed.device) {
		await pushFile(srcParsed.path, dstParsed.device, dstParsed.path, password);
	} else if (srcParsed.device && dstParsed.device) {
		// remote→remote — not supported in MVP
		throw new Error("remote-to-remote copy not supported. Pull to local first, then push.");
	} else {
		// local→local — not our job
		throw new Error("both paths are local. Use `cp` instead.");
	}
}

async function getSession(name: string, password?: string) {
	let session = getActiveSession(name);
	if (!session || !session.ready) {
		session = await createSession(name, { password });
		setActiveSession(name, session);
	}
	return session;
}

async function pullFile(deviceName: string, remotePath: string, localPath: string, password?: string): Promise<void> {
	console.log(`pulling ${deviceName}:${remotePath} → ${localPath}…`);
	const session = await getSession(deviceName, password);
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

async function pushFile(localPath: string, deviceName: string, remotePath: string, password?: string): Promise<void> {
	console.log(`pushing ${localPath} → ${deviceName}:${remotePath}…`);
	const session = await getSession(deviceName, password);

	if (!existsSync(localPath)) {
		throw new Error(`local file not found: ${localPath}`);
	}
	const isDir = statSync(localPath).isDirectory();

	// .papp.zip → unzip client-side into <dest>/<id>.papp/ then appScan, exactly like
	// Forge web (FileBrowser unzips with fflate and writes the unpacked folder). The
	// device's PappInstaller installs from .papp FOLDERS, so a raw .zip would never
	// install — both clients must unpack. appScan() asks the device to (re)install.
	if (!isDir && localPath.endsWith(".papp.zip")) {
		const id = basename(localPath).slice(0, -".papp.zip".length);
		const destDir = await resolveInto(session, remotePath, `${id}.papp`);
		const entries: Entry[] = Object.entries(unzipSync(new Uint8Array(await readFile(localPath))))
			.map(([rel, data]) => ({ rel, data }));
		console.log(`unzipping ${basename(localPath)} → ${deviceName}:${destDir}/…`);
		await replaceRemoteDir(session, destDir); // auto-replace: wipe any old version first
		const n = await writeEntries(session, destDir, entries, deviceName);
		await session.appScan(); // device-side install / registry refresh (the "protocol-level" step)
		console.log(`✓ installed ${id}.papp — ${n} files → ${deviceName}:${destDir} (appScan triggered)`);
		return;
	}

	// Local directory → recursive upload into <dest>/<basename>/, replacing any existing
	// remote folder of the same name (auto-replace).
	if (isDir) {
		const destDir = await resolveInto(session, remotePath, basename(localPath));
		const entries = await collectLocalEntries(localPath);
		console.log(`copying folder ${basename(localPath)}/ → ${deviceName}:${destDir}/ (${entries.length} files)…`);
		await replaceRemoteDir(session, destDir);
		const n = await writeEntries(session, destDir, entries, deviceName);
		console.log(`✓ ${n} files → ${deviceName}:${destDir}`);
		return;
	}

	// Single file. Resolve scp-style (into an existing dir / trailing slash, else dest
	// name). writeFile opens the remote file "wb", so an existing file is overwritten
	// (auto-replace) — no need to delete first.
	const data = new Uint8Array(await readFile(localPath));
	const targetPath = await resolveInto(session, remotePath, basename(localPath));

	// Soft mount hint: warn (don't block) if /sd isn't visible. The chunked write
	// reports the real status, so a slow listing no longer false-aborts a valid write.
	if (targetPath.startsWith("/sd")) {
		const rootEntries = await session.listDir("/");
		const sdMounted = rootEntries?.some(e => e.name === "sd" && e.isDir);
		if (rootEntries && !sdMounted) {
			console.warn(
				`⚠ "/sd" not visible in the device VFS root — ` +
				`if the write fails, check the SD card is inserted and FAT32-formatted.`
			);
		}
	}

	const ok = await session.writeFile(targetPath, data);
	if (!ok) {
		const dir = targetPath.includes("/") ? targetPath.slice(0, targetPath.lastIndexOf("/")) : "/";
		throw new Error(
			`failed to write ${targetPath} on "${deviceName}" — ${session.lastFileError}\n` +
			`  If the directory is missing: bun palanu mkdir device:${deviceName}:${dir}`
		);
	}
	console.log(`✓ ${data.length} bytes → ${deviceName}:${targetPath}`);
}

// scp-like target resolution: copy INTO remotePath (append leafName) when it ends with
// "/" or names an existing remote directory; otherwise remotePath IS the destination.
async function resolveInto(session: RemoteSession, remotePath: string, leafName: string): Promise<string> {
	if (remotePath.endsWith("/")) return remotePath.replace(/\/+$/, "") + "/" + leafName;
	if (await session.listDir(remotePath)) return remotePath.replace(/\/+$/, "") + "/" + leafName;
	return remotePath;
}

// posix dirname for device paths (always "/"-separated, regardless of host OS).
function remoteDirname(p: string): string {
	const i = p.lastIndexOf("/");
	return i <= 0 ? "/" : p.slice(0, i);
}

// mkdir -p on the device. mkdir of an existing dir is a harmless no-op; `made` memoises
// already-created paths so a big tree doesn't re-issue the same mkdir repeatedly.
async function ensureRemoteDir(session: RemoteSession, dir: string, made?: Set<string>): Promise<void> {
	let cur = "";
	for (const part of dir.split("/").filter(Boolean)) {
		cur += "/" + part;
		if (made?.has(cur)) continue;
		await session.mkdir(cur);
		made?.add(cur);
	}
}

// Auto-replace a remote directory: remove any existing one (so stale files from an old
// version don't linger), then recreate it empty.
async function replaceRemoteDir(session: RemoteSession, dir: string): Promise<void> {
	if (await session.listDir(dir)) await session.removeAll(dir);
	await ensureRemoteDir(session, dir);
}

// Write {rel,data} entries under baseDir, creating intermediate dirs as needed.
async function writeEntries(session: RemoteSession, baseDir: string, entries: Entry[], deviceName: string): Promise<number> {
	const made = new Set<string>();
	await ensureRemoteDir(session, baseDir, made);
	let count = 0;
	for (const e of entries) {
		// fflate emits directory members as zero-length names ending in "/".
		if (e.rel.endsWith("/")) {
			await ensureRemoteDir(session, baseDir + "/" + e.rel.replace(/\/+$/, ""), made);
			continue;
		}
		const full = baseDir + "/" + e.rel;
		await ensureRemoteDir(session, remoteDirname(full), made);
		const ok = await session.writeFile(full, e.data);
		if (!ok) throw new Error(`failed to write ${full} on "${deviceName}" — ${session.lastFileError}`);
		count++;
	}
	return count;
}

// Walk a local directory into posix-relative {rel,data} entries.
async function collectLocalEntries(localDir: string, prefix = ""): Promise<Entry[]> {
	const out: Entry[] = [];
	for (const name of await readdir(localDir)) {
		const lp = join(localDir, name);
		const rel = prefix ? prefix + "/" + name : name;
		if (statSync(lp).isDirectory()) out.push(...await collectLocalEntries(lp, rel));
		else out.push({ rel, data: new Uint8Array(await readFile(lp)) });
	}
	return out;
}
