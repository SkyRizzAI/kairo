// Device registry — persistent store of known devices in ~/.palanu/config.json.
// Each device has an alias, target URL, optional auth token, and last-connected timestamp.

import { readFile, writeFile, mkdir } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { homedir } from "node:os";

const CONFIG_DIR = join(homedir(), ".palanu");
const CONFIG_PATH = join(CONFIG_DIR, "config.json");

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
	await writeFile(CONFIG_PATH, JSON.stringify(reg, null, 2) + "\n", "utf-8");
}

export async function addDevice(name: string, target: string): Promise<void> {
	const reg = await loadRegistry();
	if (reg.devices[name]) {
		throw new Error(`device "${name}" already exists. Use "remove ${name}" first.`);
	}
	reg.devices[name] = { target, token: null, lastConnected: null };
	await saveRegistry(reg);
}

export async function removeDevice(name: string): Promise<void> {
	const reg = await loadRegistry();
	if (!reg.devices[name]) {
		throw new Error(`device "${name}" not found.`);
	}
	delete reg.devices[name];
	await saveRegistry(reg);
}

export async function getDevice(name: string): Promise<DeviceEntry> {
	const reg = await loadRegistry();
	const dev = reg.devices[name];
	if (!dev) throw new Error(`device "${name}" not found. Run "palanu add ${name} <target>" first.`);
	return dev;
}

export async function updateDevice(name: string, updates: Partial<DeviceEntry>): Promise<void> {
	const reg = await loadRegistry();
	if (!reg.devices[name]) throw new Error(`device "${name}" not found.`);
	reg.devices[name] = { ...reg.devices[name], ...updates };
	await saveRegistry(reg);
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
