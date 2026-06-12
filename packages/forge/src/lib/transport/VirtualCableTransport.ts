import type { ILinkTransport } from '$lib/klp/transport';

// VirtualCableTransport — the simulator "cable" (Plan 35). Loads the CLASSIC
// emscripten build (nema.js) via a <script> tag so Vite never transforms it
// (Vite's ESM transform mangles import.meta.url and breaks pthread workers).
// emscripten manages its own pthread pool from the page thread. KLP is delivered
// in-process: outbound via Module.nemaKlpOut, inbound via _nema_nlp_recv.
interface KairoModule {
	_malloc(n: number): number;
	_free(p: number): void;
	_nema_nlp_recv(ptr: number, len: number): void;
	HEAPU8: Uint8Array;
}

// emscripten (non-modularized) augments a pre-defined global Module.
declare global {
	interface Window {
		Module?: Record<string, unknown>;
	}
}

export class VirtualCableTransport implements ILinkTransport {
	// The simulator's virtual USB interface (in-process). Same role as a real
	// device's USB-CDC, just backed by the WASM instance instead of hardware.
	readonly kind = 'wasm-usb';
	#mod: KairoModule | null = null;
	#data?: (d: Uint8Array) => void;
	#state?: (c: boolean) => void;
	#ready = false;
	#loading = false;

	// Power on: load + boot the WASM firmware. Lazy — nothing runs until the user
	// presses Boot (or a remote session attaches). Idempotent.
	boot(): Promise<void> {
		if (this.#ready || this.#loading) return Promise.resolve();
		this.#loading = true;
		return this.#load();
	}

	#load(): Promise<void> {
		return new Promise((resolve, reject) => {
			window.Module = {
				locateFile: (p: string) => '/fw/' + p, // headered endpoint (COEP+CORP)
				nemaKlpOut: (bytes: Uint8Array) => this.#data?.(new Uint8Array(bytes)),
				onRuntimeInitialized: () => {
					this.#mod = window.Module as unknown as KairoModule;
					this.#ready = true;
					this.#state?.(true);
					resolve();
				}
			};
			const s = document.createElement('script');
			s.src = '/fw/nema.js';
			s.async = true;
			s.onerror = () => reject(new Error('failed to load /wasm/nema.js'));
			document.head.appendChild(s);
		});
	}

	send(d: Uint8Array) {
		const m = this.#mod;
		if (!m) return;
		const ptr = m._malloc(d.length);
		m.HEAPU8.set(d, ptr);
		m._nema_nlp_recv(ptr, d.length);
		m._free(ptr);
	}
	onData(fn: (d: Uint8Array) => void) {
		this.#data = fn;
	}
	onState(fn: (c: boolean) => void) {
		this.#state = fn;
		if (this.#ready) fn(true);
	}
	isConnected() {
		return this.#ready;
	}
	close() {
		this.#mod = null; // full teardown needs a page reload
	}
}
