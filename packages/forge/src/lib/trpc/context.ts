import type { RequestEvent } from '@sveltejs/kit';

// tRPC context. (Simulator runs client-side in WASM — no server sim bridge.)
export function createContext(event: RequestEvent) {
	return { event };
}

export type Context = Awaited<ReturnType<typeof createContext>>;
