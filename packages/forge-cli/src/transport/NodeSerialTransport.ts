// NodeSerialTransport — PLP over USB serial (serialport).
// Implements ILinkTransport from @palanu/link. Connects to a Palanu device
// exposing a CDC data port over USB. PLP frames are self-delimiting (magic + crc),
// so the stream parser on the RemoteSession side reframes them.

import { SerialPort } from "serialport";
import type { ILinkTransport } from "@palanu/link";

export class NodeSerialTransport implements ILinkTransport {
	readonly kind = "serial";
	#port: SerialPort | null = null;
	#dataCb: ((d: Uint8Array) => void) | null = null;
	#stateCb: ((c: boolean) => void) | null = null;
	#connected = false;
	#path: string;
	#baudRate: number;

	constructor(path: string, baudRate = 115200) {
		this.#path = path;
		this.#baudRate = baudRate;
	}

	boot(): Promise<void> {
		return new Promise((resolve, reject) => {
			this.#port = new SerialPort({
				path: this.#path,
				baudRate: this.#baudRate,
				autoOpen: false,
			}, (err) => {
				if (err) {
					reject(new Error(`failed to open serial port ${this.#path}: ${err.message}`));
					return;
				}
				this.#connected = true;
				this.#stateCb?.(true);
				resolve();
			});

			this.#port.on("data", (buf: Buffer) => {
				this.#dataCb?.(new Uint8Array(buf));
			});

			this.#port.on("error", (err: Error) => {
				console.error(`serial error: ${err.message}`);
				this.#connected = false;
				this.#stateCb?.(false);
			});

			this.#port.on("close", () => {
				this.#connected = false;
				this.#stateCb?.(false);
			});

			this.#port.open((err) => {
				if (err) {
					reject(new Error(`failed to open serial port ${this.#path}: ${err.message}`));
					return;
				}
				this.#connected = true;
				this.#stateCb?.(true);
				resolve();
			});
		});
	}

	send(data: Uint8Array): void {
		if (!this.#port || !this.#port.isOpen) return;
		this.#port.write(Buffer.from(data));
	}

	onData(fn: (d: Uint8Array) => void): void {
		this.#dataCb = fn;
	}

	onState(fn: (c: boolean) => void): void {
		this.#stateCb = fn;
		fn(this.#connected);
	}

	isConnected(): boolean {
		return this.#connected && (this.#port?.isOpen ?? false);
	}

	close(): void {
		if (this.#port) {
			this.#port.close();
			this.#port = null;
		}
		this.#connected = false;
		this.#stateCb?.(false);
	}
}
