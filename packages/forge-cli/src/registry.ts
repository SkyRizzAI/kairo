// Device registry — persistent store of known devices in ~/.palanu/config.json.
// Each device has an alias, target URL, optional auth token, and last-connected timestamp.

import { readFile, writeFile, mkdir, rename } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { homedir } from "node:os";

const CONFIG_DIR = join(homedir(), ".palanu");
const CONFIG_PATH = join(CONFIG_DIR, "config.json");

// Serialise read-modify-write of the registry. Without this, two concurrent
// updateDevice() calls (e.g. the lastConnected stamp and the fire-and-forget token
// save during connect) interleave their writeFile()s and corrupt config.json.
let writeChain: Promise<unknown> = Promise.resolve();
function withRegistryLock<T>(fn: () => Promise<T>): Promise<T> {
	const run = writeChain.then(fn, fn);
	writeChain = run.then(
		() => {},
		() => {}
	);
	return run;
}

export interface DeviceEntry {
	target: string;
	token: string | null;
	lastConnected: string | null;
}

export interface Registry {
	devices: Record<string, DeviceEntry>;
}

function defaultRegistry(): Registry {
	return { devices: {} };
}

export async function loadRegistry(): Promise<Registry> {
	if (!existsSync(CONFIG_PATH)) return defaultRegistry();
	try {
		const raw = await readFile(CONFIG_PATH, "utf-8");
		const data = JSON.parse(raw);
		if (!data.devices) return defaultRegistry();
		return data as Registry;
	} catch {
		return defaultRegistry();
	}
}

export async function saveRegistry(reg: Registry): Promise<void> {
	await mkdir(CONFIG_DIR, { recursive: true });
	// Atomic write: a partial/interleaved write can never leave a corrupt config —
	// the rename is atomic, so readers see either the old or the new file whole.
	const tmp = `${CONFIG_PATH}.${process.pid}.tmp`;
	await writeFile(tmp, JSON.stringify(reg, null, 2) + "\n", "utf-8");
	await rename(tmp, CONFIG_PATH);
}

export async function addDevice(name: string, target: string): Promise<void> {
	await withRegistryLock(async () => {
		const reg = await loadRegistry();
		if (reg.devices[name]) {
			throw new Error(`device "${name}" already exists. Use "remove ${name}" first.`);
		}
		reg.devices[name] = { target, token: null, lastConnected: null };
		await saveRegistry(reg);
	});
}

export async function removeDevice(name: string): Promise<void> {
	await withRegistryLock(async () => {
		const reg = await loadRegistry();
		if (!reg.devices[name]) {
			throw new Error(`device "${name}" not found.`);
		}
		delete reg.devices[name];
		await saveRegistry(reg);
	});
}

export async function getDevice(name: string): Promise<DeviceEntry> {
	const reg = await loadRegistry();
	const dev = reg.devices[name];
	if (!dev) throw new Error(`device "${name}" not found. Run "palanu add ${name} <target>" first.`);
	return dev;
}

export async function updateDevice(name: string, updates: Partial<DeviceEntry>): Promise<void> {
	// Locked: load→modify→save runs atomically vs. other concurrent updates.
	await withRegistryLock(async () => {
		const reg = await loadRegistry();
		if (!reg.devices[name]) return; // device removed meanwhile — nothing to update
		reg.devices[name] = { ...reg.devices[name], ...updates };
		await saveRegistry(reg);
	});
}

export async function listDevices(): Promise<Record<string, DeviceEntry>> {
	const reg = await loadRegistry();
	return reg.devices;
}

// In-memory connected sessions — not persisted.
const activeSessions = new Map<string, import("@palanu/link").RemoteSession>();

export function setActiveSession(name: string, session: import("@palanu/link").RemoteSession): void {
	activeSessions.set(name, session);
}

export function getActiveSession(name: string): import("@palanu/link").RemoteSession | null {
	return activeSessions.get(name) ?? null;
}

export function clearActiveSession(name: string): void {
	const session = activeSessions.get(name);
	if (session) {
		session.close();
		activeSessions.delete(name);
	}
}

export function isConnected(name: string): boolean {
	const session = activeSessions.get(name);
	return session?.ready ?? false;
}
