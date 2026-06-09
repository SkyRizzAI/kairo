import type { ILinkTransport } from '$lib/klp/transport';

// SerialTransport — KLP over Web Serial / USB-CDC (Plan 35). Connects to a Kairo
// device exposing a CDC data port (Plan 34 USB). KLP frames are self-delimiting
// (magic + crc), so the stream parser on the RemoteSession side reframes them.
// Requires a real device + Chrome/Edge.
export class SerialTransport implements ILinkTransport {
	readonly kind = 'usb';
	#data?: (d: Uint8Array) => void;
	#state?: (c: boolean) => void;
	#connected = false;
	#port?: SerialPort;
	#writer?: WritableStreamDefaultWriter<Uint8Array>;

	static available() {
		return typeof navigator !== 'undefined' && !!navigator.serial;
	}

	async connect(baudRate = 921600) {
		const port = await navigator.serial.requestPort();
		await port.open({ baudRate });
		this.#port = port;
		this.#writer = port.writable!.getWriter();
		this.#connected = true;
		this.#state?.(true);
		this.#readLoop(port);
	}

	async #readLoop(port: SerialPort) {
		const reader = port.readable!.getReader();
		try {
			for (;;) {
				const { value, done } = await reader.read();
				if (done) break;
				if (value) this.#data?.(value);
			}
		} catch {
			/* port closed */
		} finally {
			reader.releaseLock();
			this.#connected = false;
			this.#state?.(false);
		}
	}

	send(d: Uint8Array) {
		this.#writer?.write(d).catch(() => {});
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
		this.#writer?.releaseLock();
		this.#port?.close().catch(() => {});
		this.#connected = false;
	}
}
