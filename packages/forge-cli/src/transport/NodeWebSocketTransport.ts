// NodeWebSocketTransport — PLP over WebSocket (ws).
// Implements ILinkTransport from @palanu/link. Connects to a Palanu device
// running the esp_http_server WS endpoint at ws://<host>:8477/plp over WiFi.

import WebSocket from "ws";
import type { ILinkTransport } from "@palanu/link";

export class NodeWebSocketTransport implements ILinkTransport {
	readonly kind = "ws";
	#ws: WebSocket | null = null;
	#dataCb: ((d: Uint8Array) => void) | null = null;
	#stateCb: ((c: boolean) => void) | null = null;
	#connected = false;
	#url: string;

	constructor(hostOrUrl: string, port = 8477, path = "/plp") {
		this.#url = NodeWebSocketTransport.normalizeUrl(hostOrUrl, port, path);
	}

	// Accept "host", "host:port", "host.local", or a full "ws://..." URL.
	// Mirrors Forge web's WebSocketTransport.normalizeUrl.
	static normalizeUrl(host: string, port = 8477, path = "/plp"): string {
		host = host.trim();
		if (host.startsWith("ws://") || host.startsWith("wss://")) return host;
		host = host.replace(/^https?:\/\//, "").replace(/\/+$/, "");
		if (/:\d+$/.test(host)) return `ws://${host}${path}`;
		return `ws://${host}:${port}${path}`;
	}

	get url(): string {
		return this.#url;
	}

	boot(): Promise<void> {
		return new Promise((resolve, reject) => {
			this.#ws = new WebSocket(this.#url);

			this.#ws.on("open", () => {
				this.#connected = true;
				this.#stateCb?.(true);
				resolve();
			});

			this.#ws.on("message", (data: Buffer) => {
				this.#dataCb?.(new Uint8Array(data));
			});

			this.#ws.on("error", (err: Error) => {
				if (!this.#connected) {
					reject(new Error(`WebSocket connection failed: ${err.message}`));
					return;
				}
				console.error(`WebSocket error: ${err.message}`);
			});

			this.#ws.on("close", () => {
				this.#connected = false;
				this.#stateCb?.(false);
			});
		});
	}

	send(data: Uint8Array): void {
		if (!this.#ws || this.#ws.readyState !== WebSocket.OPEN) return;
		this.#ws.send(Buffer.from(data));
	}

	onData(fn: (d: Uint8Array) => void): void {
		this.#dataCb = fn;
	}

	onState(fn: (c: boolean) => void): void {
		this.#stateCb = fn;
		fn(this.#connected);
	}

	isConnected(): boolean {
		return this.#connected && (this.#ws?.readyState === WebSocket.OPEN);
	}

	close(): void {
		if (this.#ws) {
			this.#ws.close();
			this.#ws = null;
		}
		this.#connected = false;
		this.#stateCb?.(false);
	}
}
