// Token store abstraction — lets RemoteSession work in both browser (localStorage)
// and Node (file-backed) environments without hardcoding either.
//
// Plan 77: decouple RemoteSession from localStorage so @palanu/link is isomorphic.
// Forge web uses LocalStorageTokenStore (default); Forge CLI uses FileTokenStore.

export interface ITokenStore {
	get(key: string): string | null;
	set(key: string, val: string): void;
	remove(key: string): void;
}

export const DEFAULT_TOKEN_KEY = 'palanu.remote.token';

/**
 * Browser localStorage-backed token store. Guarded so it no-ops in non-browser
 * environments (Node) instead of throwing — backward compatible with the
 * original RemoteSession behavior.
 */
export class LocalStorageTokenStore implements ITokenStore {
	get(key: string): string | null {
		try {
			return typeof localStorage !== 'undefined' ? localStorage.getItem(key) : null;
		} catch {
			return null;
		}
	}
	set(key: string, val: string): void {
		try {
			localStorage?.setItem(key, val);
		} catch {
			/* ignore */
		}
	}
	remove(key: string): void {
		try {
			localStorage?.removeItem(key);
		} catch {
			/* ignore */
		}
	}
}

/**
 * In-memory token store — for tests and environments with no persistent storage.
 */
export class MemoryTokenStore implements ITokenStore {
	#map = new Map<string, string>();
	get(key: string): string | null {
		return this.#map.get(key) ?? null;
	}
	set(key: string, val: string): void {
		this.#map.set(key, val);
	}
	remove(key: string): void {
		this.#map.delete(key);
	}
}
