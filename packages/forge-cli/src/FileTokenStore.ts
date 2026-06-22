// FileTokenStore — ITokenStore implementation backed by ~/.palanu/config.json.
// Stores auth tokens per-device so the CLI can reconnect without re-entering password.

import type { ITokenStore } from "@palanu/link";
import { loadRegistry, saveRegistry, getDevice, updateDevice } from "./registry.js";

export class FileTokenStore implements ITokenStore {
	#deviceName: string;

	constructor(deviceName: string) {
		this.#deviceName = deviceName;
	}

	get(key: string): string | null {
		// Synchronous read from registry — but registry is async.
		// For the CLI, tokens are loaded before the session starts and passed
		// via the constructor. This method returns null (token is pre-loaded).
		// The actual token is read in `connect` and passed to RemoteSession
		// via the tokenStore. FileTokenStore is a fallback for mid-session saves.
		return null;
	}

	async set(key: string, val: string): Promise<void> {
		await updateDevice(this.#deviceName, { token: val });
	}

	async remove(key: string): Promise<void> {
		await updateDevice(this.#deviceName, { token: null });
	}

	// Load the saved token for this device from the registry (async, call before session).
	static async loadToken(deviceName: string): Promise<string | null> {
		const dev = await getDevice(deviceName);
		return dev.token;
	}
}

// Synchronous wrapper that holds a pre-loaded token — for RemoteSession which
// calls ITokenStore.get() synchronously during the auth handshake.
export class PreloadedTokenStore implements ITokenStore {
	#token: string | null;
	#deviceName: string;

	constructor(deviceName: string, token: string | null) {
		this.#deviceName = deviceName;
		this.#token = token;
	}

	get(_key: string): string | null {
		return this.#token;
	}

	set(_key: string, val: string): void {
		this.#token = val;
		// Persist async (fire-and-forget — RemoteSession calls this after AUTH_OK)
		updateDevice(this.#deviceName, { token: val }).catch(() => {});
	}

	remove(_key: string): void {
		this.#token = null;
		updateDevice(this.#deviceName, { token: null }).catch(() => {});
	}
}
