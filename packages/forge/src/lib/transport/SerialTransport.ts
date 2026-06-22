import type { ILinkTransport } from '@palanu/link';

// SerialTransport — PLP over Web Serial / USB-CDC (Plan 35). Connects to a Palanu
// device exposing a CDC data port (Plan 34 USB). PLP frames are self-delimiting
// (magic + crc), so the stream parser on the RemoteSession side reframes them.
// Requires a real device + Chrome/Edge.
export class SerialTransport implements ILinkTransport {
	readonly kind = 'usb';
	#data?: (d: Uint8Array) => void;
	#state?: (c: boolean) => void;
	#connected = false;
	#port?: SerialPort;
	#writer?: WritableStreamDefaultWriter<Uint8Array>;
	#reader?: ReadableStreamDefaultReader<Uint8Array>;
	#readDone?: Promise<void>;

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
		this.#readDone = this.#readLoop(port);
	}

	async #readLoop(port: SerialPort) {
		const reader = port.readable!.getReader();
		this.#reader = reader;
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
	// Fully release the port: cancel the reader (unblocks + ends the read loop),
	// wait for it to drop its stream lock, then close. Skipping any of these
	// leaves the OS port held → the next connect fails with "port already open".
	async close() {
		this.#connected = false;
		try {
			await this.#reader?.cancel();
		} catch {
			/* already closed */
		}
		await this.#readDone?.catch(() => {});
		try {
			this.#writer?.releaseLock();
		} catch {
			/* lock already released */
		}
		try {
			await this.#port?.close();
		} catch {
			/* port already closed/lost */
		}
		this.#port = undefined;
	}
}
