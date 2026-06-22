import type { ILinkTransport } from '@palanu/link';

// WebSocketTransport — PLP over WebSocket (Plan 75). Connects to a Palanu device
// running the esp_http_server WS endpoint at ws://<host>:8477/plp over WiFi, so
// Forge web can remote the device over the network (not just USB/BLE). PLP frames
// ride inside binary WS messages; the RemoteSession FrameParser reframes them.
export class WebSocketTransport implements ILinkTransport {
	readonly kind = 'net';
	#url: string;
	#ws?: WebSocket;
	#data?: (d: Uint8Array) => void;
	#state?: (c: boolean) => void;
	#connected = false;

	constructor(hostOrUrl: string, port = 8477, path = '/plp') {
		this.#url = WebSocketTransport.normalizeUrl(hostOrUrl, port, path);
	}

	static available() {
		return typeof WebSocket !== 'undefined';
	}

	// Accept "host", "host:port", "host.local", or a full "ws://..." URL.
	static normalizeUrl(host: string, port = 8477, path = '/plp') {
		host = host.trim();
		if (host.startsWith('ws://') || host.startsWith('wss://')) return host;
		host = host.replace(/^https?:\/\//, '').replace(/\/+$/, '');
		if (/:\d+$/.test(host)) return `ws://${host}${path}`;
		return `ws://${host}:${port}${path}`;
	}

	get url() {
		return this.#url;
	}

	boot() {
		return new Promise<void>((resolve, reject) => {
			let settled = false;
			const ws = new WebSocket(this.#url);
			ws.binaryType = 'arraybuffer';
			this.#ws = ws;
			ws.onopen = () => {
				this.#connected = true;
				this.#state?.(true);
				settled = true;
				resolve();
			};
			ws.onclose = () => {
				this.#connected = false;
				this.#state?.(false);
				if (!settled) {
					settled = true;
					reject(new Error(`WebSocket closed before open: ${this.#url}`));
				}
			};
			ws.onerror = () => {
				if (!settled) {
					settled = true;
					reject(new Error(`WebSocket connect failed: ${this.#url}`));
				}
			};
			ws.onmessage = (e) => {
				if (e.data instanceof ArrayBuffer) {
					this.#data?.(new Uint8Array(e.data));
				} else if (e.data instanceof Blob) {
					e.data.arrayBuffer().then((b) => this.#data?.(new Uint8Array(b)));
				}
			};
		});
	}

	send(d: Uint8Array) {
		if (this.#ws && this.#ws.readyState === WebSocket.OPEN) {
			// Copy into a fresh ArrayBuffer-backed view (the source may be reused, and
			// WebSocket.send rejects SharedArrayBuffer-backed views in TS strict mode).
			const copy = new Uint8Array(d.byteLength);
			copy.set(d);
			this.#ws.send(copy);
		}
	}
	onData(fn: (d: Uint8Array) => void) {
		this.#data = fn;
	}
	onState(fn: (c: boolean) => void) {
		this.#state = fn;
		if (this.#connected) fn(true);
	}
	isConnected() {
		return this.#connected;
	}
	close() {
		this.#connected = false;
		try {
			this.#ws?.close();
		} catch {
			/* already closed */
		}
		this.#ws = undefined;
	}
}
