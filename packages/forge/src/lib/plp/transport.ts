// PLP transport abstraction (Forge side).
//
// The remote protocol (PLP) is ALWAYS the same. What changes per connection is
// the transport — the "cable". A physical device uses BLE or USB; the WASM
// simulator uses a "virtual cable" (Web Worker postMessage). They are
// interchangeable behind this one interface.
export interface ILinkTransport {
	/** Stable id for UI (e.g. "wasm", "ble", "serial"). */
	readonly kind: string;
	/** Open the cable (power on). For the WASM cable this loads + boots the
	 *  firmware; for BLE/USB it triggers the connect flow. Idempotent. Optional —
	 *  transports that connect eagerly (e.g. loopback) may omit it. */
	boot?(): void | Promise<void>;
	/** Send one already-encoded PLP frame (or framed bytes). */
	send(data: Uint8Array): void;
	/** Register a receiver for inbound bytes (feed to a FrameParser). */
	onData(fn: (data: Uint8Array) => void): void;
	/** Register connection-state changes. */
	onState(fn: (connected: boolean) => void): void;
	isConnected(): boolean;
	close(): void;
}

/**
 * In-process loopback pair — two transports wired back-to-back. Used in tests
 * and as the model for the virtual cable: whatever A sends, B receives.
 */
export function loopbackPair(kind = 'loopback'): [ILinkTransport, ILinkTransport] {
	const make = (): ILinkTransport & { _emit: (d: Uint8Array) => void; _peer?: ILinkTransport & { _emit: (d: Uint8Array) => void } } => {
		let dataCb: ((d: Uint8Array) => void) | null = null;
		let stateCb: ((c: boolean) => void) | null = null;
		let connected = true;
		return {
			kind,
			send(d) {
				this._peer?._emit(d);
			},
			onData(fn) {
				dataCb = fn;
			},
			onState(fn) {
				stateCb = fn;
				fn(connected);
			},
			isConnected() {
				return connected;
			},
			close() {
				connected = false;
				stateCb?.(false);
			},
			_emit(d) {
				dataCb?.(d);
			}
		};
	};
	const a = make();
	const b = make();
	a._peer = b;
	b._peer = a;
	return [a, b];
}
