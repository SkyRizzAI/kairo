import type { ILinkTransport } from '$lib/klp/transport';
import { KLP_SERVICE, KLP_CHAR_TX, KLP_CHAR_RX } from '$lib/klp/uuids';

// BleTransport — KLP over Web Bluetooth (Plan 35). Connects to a Kairo device
// that advertises the KLP GATT service (Plan 34). Same ILinkTransport interface
// as the WASM virtual cable, so RemoteSession is identical. Requires a real,
// flashed Kairo device + Chrome/Edge.
export class BleTransport implements ILinkTransport {
	readonly kind = 'ble';
	#data?: (d: Uint8Array) => void;
	#state?: (c: boolean) => void;
	#connected = false;
	#rx?: BluetoothRemoteGATTCharacteristic; // host → device (write)

	static available() {
		return typeof navigator !== 'undefined' && !!navigator.bluetooth;
	}

	async connect() {
		const dev = await navigator.bluetooth.requestDevice({
			filters: [{ services: [KLP_SERVICE] }]
		});
		dev.addEventListener('gattserverdisconnected', () => {
			this.#connected = false;
			this.#state?.(false);
		});
		const gatt = await dev.gatt!.connect();
		const svc = await gatt.getPrimaryService(KLP_SERVICE);
		const tx = await svc.getCharacteristic(KLP_CHAR_TX);
		this.#rx = await svc.getCharacteristic(KLP_CHAR_RX);
		await tx.startNotifications();
		tx.addEventListener('characteristicvaluechanged', (e) => {
			const v = (e.target as BluetoothRemoteGATTCharacteristic).value;
			if (v) this.#data?.(new Uint8Array(v.buffer));
		});
		this.#connected = true;
		this.#state?.(true);
	}

	send(d: Uint8Array) {
		// GATT writes cap at ~512B; KLP frames fit for control/input. Copy into a
		// fresh ArrayBuffer-backed view (GATT rejects SharedArrayBuffer-backed data).
		const buf = new Uint8Array(d.length);
		buf.set(d);
		this.#rx?.writeValueWithoutResponse(buf).catch(() => {});
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
	}
}
