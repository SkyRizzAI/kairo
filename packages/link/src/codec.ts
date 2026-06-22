// Palanu Link Protocol (PLP) — wire codec.
//
// One protocol, many transports. The SAME frames travel over BLE, USB-Serial,
// or the simulator "virtual cable" (postMessage). Only the transport differs.
//
// Frame (self-delimiting + sync byte so a byte stream can resync after noise):
//   [magic:0xAB][chan:1][flags:1][len:2 LE][payload:len][crc8:1]
//
// This file is the canonical TS codec; the C++ codec (firmware) mirrors it
// byte-for-byte and they share the test vectors.

export enum Channel {
	Control = 0x00, // HELLO / ACK / REJECT / PING / PONG
	Screen = 0x01, // framebuffer (1-bit, optionally RLE)
	Input = 0x02, // action / pointer
	Log = 0x03, // log entries
	System = 0x04, // device info, power (restart/sleep/shutdown)
	Ota = 0x05, // firmware update chunks
	Ext = 0x06, // host→device sim-control commands (inject event, wifi router)
	Event = 0x07, // device→host EventBus stream (Events panel)
	Cli = 0x08, // terminal: host sends a command line, device streams text + EOT
	File = 0x09 // filesystem request/response (list/read/write/mkdir/remove)
}

export const Flags = {
	None: 0,
	FragMore: 1 << 0, // payload continues in the next frame
	Compressed: 1 << 1 // payload is RLE-encoded
} as const;

export interface Frame {
	channel: number;
	flags: number;
	payload: Uint8Array;
}

const MAGIC = 0xab; // sync byte — lets a noisy byte stream resync after CRC loss
const HEADER = 5; // magic + chan + flags + len(2)

// CRC-8/SMBus (poly 0x07, init 0x00).
export function crc8(data: Uint8Array, start = 0, end = data.length): number {
	let crc = 0;
	for (let i = start; i < end; i++) {
		crc ^= data[i];
		for (let b = 0; b < 8; b++) crc = crc & 0x80 ? ((crc << 1) ^ 0x07) & 0xff : (crc << 1) & 0xff;
	}
	return crc;
}

/** Encode one PLP frame to bytes. */
export function encodeFrame(channel: number, payload: Uint8Array, flags = 0): Uint8Array {
	const len = payload.length;
	const out = new Uint8Array(HEADER + len + 1);
	out[0] = MAGIC;
	out[1] = channel & 0xff;
	out[2] = flags & 0xff;
	out[3] = len & 0xff;
	out[4] = (len >> 8) & 0xff;
	out.set(payload, HEADER);
	out[HEADER + len] = crc8(out, 0, HEADER + len);
	return out;
}

/**
 * Stream parser — feed arbitrary byte chunks, get back complete frames.
 * Works for stream transports (serial) and message transports (one frame per
 * message still parses fine). Resyncs by dropping a byte on CRC mismatch.
 */
export class FrameParser {
	#buf = new Uint8Array(0);

	push(chunk: Uint8Array): Frame[] {
		const merged = new Uint8Array(this.#buf.length + chunk.length);
		merged.set(this.#buf, 0);
		merged.set(chunk, this.#buf.length);
		this.#buf = merged;

		const frames: Frame[] = [];
		let off = 0;
		while (off < this.#buf.length) {
			if (this.#buf[off] !== MAGIC) {
				off++; // scan for the sync byte
				continue;
			}
			if (this.#buf.length - off < HEADER) break; // wait for header
			const len = this.#buf[off + 3] | (this.#buf[off + 4] << 8);
			const total = HEADER + len + 1;
			if (this.#buf.length - off < total) break; // wait for full frame
			const crc = this.#buf[off + HEADER + len];
			if (crc8(this.#buf, off, off + HEADER + len) === crc) {
				frames.push({
					channel: this.#buf[off + 1],
					flags: this.#buf[off + 2],
					payload: this.#buf.slice(off + HEADER, off + HEADER + len)
				});
				off += total;
			} else {
				off += 1; // bad CRC → scan for next sync byte
			}
		}
		this.#buf = this.#buf.slice(off);
		return frames;
	}

	reset() {
		this.#buf = new Uint8Array(0);
	}
}

// ── RLE for 1-bit framebuffers (w*h bytes, each 0 or 1) ──
// Pairs of [count:1][value:1]; runs longer than 255 are split. Great for the
// mostly-blank monochrome Palanu UI.
export function rleEncode(px: Uint8Array): Uint8Array {
	const out: number[] = [];
	let i = 0;
	while (i < px.length) {
		const v = px[i] ? 1 : 0;
		let run = 1;
		while (i + run < px.length && (px[i + run] ? 1 : 0) === v && run < 255) run++;
		out.push(run, v);
		i += run;
	}
	return new Uint8Array(out);
}

export function rleDecode(data: Uint8Array, expectedLen?: number): Uint8Array {
	const out: number[] = [];
	for (let i = 0; i + 1 < data.length; i += 2) {
		const run = data[i];
		const v = data[i + 1];
		for (let j = 0; j < run; j++) out.push(v);
	}
	const arr = new Uint8Array(out);
	if (expectedLen !== undefined && arr.length !== expectedLen) {
		// length mismatch is a protocol error; caller may choose to ignore
	}
	return arr;
}
