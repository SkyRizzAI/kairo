import { Channel, Flags, encodeFrame, FrameParser, rleDecode } from '$lib/plp/codec';
import type { ILinkTransport } from '$lib/plp/transport';

export interface ScreenFrame {
	w: number;
	h: number;
	px: Uint8Array; // w*h, 0/1
}
export interface LogEntry {
	level: number;
	component: string;
	message: string;
}
export interface EventEntry {
	name: string;
	fields: Record<string, string>;
}
// CLI output chunk. `text` is a line of output; `done` marks end-of-command (the
// device sent EOT) so the terminal can re-enable its prompt; `prompt` carries the
// device's current working directory (Plan 44) for a shell-like prompt.
export interface CliChunk {
	sid?: number; // session id (Plan 45) — terminals filter to their own session
	text?: string;
	done?: boolean;
	prompt?: string;
}
// One directory entry from the FILE channel (mirrors firmware FsEntry).
export interface FileEntry {
	name: string;
	isDir: boolean;
	size: number;
}

// Board profile — the device's physical layout (SYSTEM GetInfo reply, Plan 33).
// Mirrors firmware BoardProfile/ComponentDef; coordinates are normalized 0–1
// over the board rect, `w`/`h` on the profile are the physical aspect (mm).
export interface BoardComponent {
	id: number;
	label: string;
	type: 'display' | 'button' | 'led' | 'sensor' | 'speaker' | 'mic' | 'camera' | 'port' | 'other';
	key?: number; // input Key to send when this (button) is pressed remotely
	x: number;
	y: number;
	w: number;
	h: number;
}
export interface BoardProfile {
	id: string;
	name: string;
	w: number;
	h: number;
	components: BoardComponent[];
}

const HELLO = 0x01;
const ACK = 0x02;
const GET_INFO = 0x01; // SYSTEM channel opcode

export const Power = { Restart: 0x10, Sleep: 0x11, Shutdown: 0x12 } as const;
export const Key = { Up: 1, Down: 2, Left: 3, Right: 4, Select: 5, Cancel: 6 } as const;

// Browser keyboard → Palanu Key. Shared by every view that forwards keystrokes to
// a device (/remote SessionView, /simulator) so the mapping lives in one place.
export const KEY_MAP: Record<string, number> = {
	ArrowUp: Key.Up,
	ArrowDown: Key.Down,
	ArrowLeft: Key.Left,
	ArrowRight: Key.Right,
	Enter: Key.Select,
	' ': Key.Select,
	Escape: Key.Cancel,
	Backspace: Key.Cancel
};

// "WxH" of a screen frame, or "—" before the first frame. Shared by the headers
// of /remote and /simulator so the formatting stays identical.
export function frameDims(frame: ScreenFrame | null): string {
	return frame ? `${frame.w}×${frame.h}` : '—';
}
const ExtOp = { InjectEvent: 0x01, WifiSetNetworks: 0x02, AppInstall: 0x03 } as const;
const FileOp = { List: 0x01, Read: 0x03, Write: 0x04, Mkdir: 0x05, Remove: 0x06 } as const;

type Listeners = {
	screen: Set<(f: ScreenFrame) => void>;
	log: Set<(l: LogEntry) => void>;
	event: Set<(e: EventEntry) => void>;
	ready: Set<() => void>;
	profile: Set<(p: BoardProfile) => void>;
	cli: Set<(c: CliChunk) => void>;
};

// RemoteSession — transport-agnostic client of a Palanu device (WASM sim or real
// hardware) over PLP. Multi-listener so /simulator and /remote can both observe
// the SAME device without clobbering each other.
export class RemoteSession {
	#t: ILinkTransport;
	#parser = new FrameParser();
	#ready = false;
	#profile: BoardProfile | null = null;
	#helloTimer: ReturnType<typeof setInterval> | null = null;
	// FILE channel is request/response; one queue of pending resolvers per opcode.
	// Sequential UI usage → FIFO correlation is enough.
	#filePending: Record<number, ((r: { status: number; rest: Uint8Array } | null) => void)[]> = {};
	// OTA is strictly sequential (Begin→Data×N→End), so one in-flight resolver.
	#otaPending: ((r: { status: number; written: number; proto: number } | null) => void) | null = null;
	#l: Listeners = {
		screen: new Set(),
		log: new Set(),
		event: new Set(),
		ready: new Set(),
		profile: new Set(),
		cli: new Set()
	};

	constructor(t: ILinkTransport) {
		this.#t = t;
		t.onData((d) => this.#onBytes(d));
		t.onState((c) => {
			if (c) this.#hello();
		});
		if (t.isConnected()) this.#hello();
	}

	get ready() {
		return this.#ready;
	}

	// Last board profile received from the device (null until GetInfo answered).
	get profile() {
		return this.#profile;
	}

	// Power on the cable (load/boot the WASM firmware, or open BLE/USB). The
	// onState callback drives the handshake once the transport reports connected.
	boot(): void | Promise<void> {
		return this.#t.boot?.();
	}

	// Tear the session down: stop the handshake retry and close the cable so the
	// transport's resources (e.g. the Web Serial port) are actually released.
	close() {
		if (this.#helloTimer) {
			clearInterval(this.#helloTimer);
			this.#helloTimer = null;
		}
		this.#ready = false;
		this.#t.close();
	}

	on<K extends keyof Listeners>(kind: K, fn: Listeners[K] extends Set<infer F> ? F : never) {
		(this.#l[kind] as Set<unknown>).add(fn as unknown);
		return () => (this.#l[kind] as Set<unknown>).delete(fn as unknown);
	}

	#emit<K extends keyof Listeners>(kind: K, ...args: unknown[]) {
		for (const fn of this.#l[kind] as Set<(...a: unknown[]) => void>) fn(...args);
	}

	#hello() {
		const send = () => {
			const p = new Uint8Array(17);
			p[0] = HELLO;
			this.#t.send(encodeFrame(Channel.Control, p));
		};
		send();
		if (this.#helloTimer) return;
		this.#helloTimer = setInterval(() => {
			if (this.#ready) {
				if (this.#helloTimer) clearInterval(this.#helloTimer);
				this.#helloTimer = null;
				return;
			}
			send();
		}, 300);
	}

	#onBytes(d: Uint8Array) {
		for (const f of this.#parser.push(d)) this.#handle(f);
	}

	#handle(f: { channel: number; flags: number; payload: Uint8Array }) {
		if (f.channel === Channel.Control) {
			if (f.payload[0] === ACK) {
				this.#ready = true;
				this.#emit('ready');
				// Handshake done → ask for the board profile (SYSTEM GetInfo).
				this.#t.send(encodeFrame(Channel.System, new Uint8Array([GET_INFO])));
			}
			return;
		}
		if (f.channel === Channel.System) {
			// [GET_INFO][board-profile json] — see RemoteService::dispatch.
			if (f.payload[0] !== GET_INFO) return;
			try {
				const json = new TextDecoder().decode(f.payload.subarray(1));
				const p = JSON.parse(json) as BoardProfile;
				p.components ??= [];
				this.#profile = p;
				this.#emit('profile', p);
			} catch {
				/* malformed profile — ignore, UI keeps the generic layout */
			}
			return;
		}
		if (f.channel === Channel.File) {
			// reply: [op][status][path\0][extra...]. Resolve the oldest request of op.
			const p = f.payload;
			const op = p[0];
			const status = p[1];
			let i = 2;
			while (i < p.length && p[i]) i++; // skip path
			i++;
			const rest = p.subarray(i);
			this.#filePending[op]?.shift()?.({ status, rest });
			return;
		}
		if (f.channel === Channel.Cli) {
			// device→host CLI frame: [sid][rest]. rest is an 0x04 (EOT) marker, a
			// prompt frame [0x01]<cwd> (Plan 44), or output text. sid (Plan 45)
			// routes the chunk to the terminal that owns that session.
			if (f.payload.length < 1) return;
			const sid = f.payload[0];
			const rest = f.payload.slice(1);
			if (rest.length === 1 && rest[0] === 0x04) {
				this.#emit('cli', { sid, done: true } as CliChunk);
			} else if (rest.length >= 1 && rest[0] === 0x01) {
				const prompt = new TextDecoder().decode(rest.slice(1));
				this.#emit('cli', { sid, prompt } as CliChunk);
			} else {
				const text = new TextDecoder().decode(rest);
				this.#emit('cli', { sid, text } as CliChunk);
			}
			return;
		}
		if (f.channel === Channel.Ota) {
			// device→host: [op][status][written:4 LE]([proto:1] on the Begin ack)
			const p = f.payload;
			const written = p.length >= 6 ? p[2] | (p[3] << 8) | (p[4] << 16) | (p[5] * 0x1000000) : 0;
			const proto = p.length >= 7 ? p[6] : 0; // 0 = stale firmware (no version byte)
			const cb = this.#otaPending;
			this.#otaPending = null;
			cb?.({ status: p[1] ?? 1, written, proto });
			return;
		}
		if (f.channel === Channel.Screen) {
			const p = f.payload;
			const w = p[0] | (p[1] << 8);
			const h = p[2] | (p[3] << 8);
			const body = p.subarray(4);
			const px = f.flags & Flags.Compressed ? rleDecode(body, body.length) : body;
			this.#emit('screen', { w, h, px } as ScreenFrame);
		} else if (f.channel === Channel.Log) {
			const p = f.payload;
			const level = p[0];
			let i = 1;
			let c = '';
			while (i < p.length && p[i]) c += String.fromCharCode(p[i++]);
			i++;
			let m = '';
			while (i < p.length && p[i]) m += String.fromCharCode(p[i++]);
			this.#emit('log', { level, component: c, message: m } as LogEntry);
		} else if (f.channel === Channel.Event) {
			// [name\0][key\0val\0]...
			const p = f.payload;
			let i = 0;
			const read = () => {
				let s = '';
				while (i < p.length && p[i]) s += String.fromCharCode(p[i++]);
				i++;
				return s;
			};
			const name = read();
			const fields: Record<string, string> = {};
			while (i < p.length) {
				const k = read();
				if (i > p.length) break;
				const v = read();
				if (k) fields[k] = v;
			}
			this.#emit('event', { name, fields } as EventEntry);
		}
	}

	sendKey(key: number) {
		this.#t.send(encodeFrame(Channel.Input, new Uint8Array([key])));
	}
	// Run a CLI command line in session `sid`; output arrives via 'cli' (Plan 45).
	// Frame payload = [sid][line].
	sendCli(sid: number, line: string) {
		const lineBytes = new TextEncoder().encode(line);
		const payload = new Uint8Array(1 + lineBytes.length);
		payload[0] = sid & 0xff;
		payload.set(lineBytes, 1);
		this.#t.send(encodeFrame(Channel.Cli, payload));
	}

	// ── FILE channel (request/response) ──
	#fileReq(op: number, body: Uint8Array): Promise<{ status: number; rest: Uint8Array } | null> {
		return new Promise((resolve) => {
			const entry = (r: { status: number; rest: Uint8Array } | null) => {
				clearTimeout(timer);
				resolve(r);
			};
			const timer = setTimeout(() => {
				const q = this.#filePending[op];
				const k = q?.indexOf(entry) ?? -1;
				if (k >= 0) q.splice(k, 1);
				resolve(null); // no reply (device offline / unsupported) → null
			}, 5000);
			(this.#filePending[op] ??= []).push(entry);
			this.#t.send(encodeFrame(Channel.File, body));
		});
	}

	async listDir(path: string): Promise<FileEntry[] | null> {
		const pb = new TextEncoder().encode(path);
		const body = new Uint8Array(1 + pb.length);
		body[0] = FileOp.List;
		body.set(pb, 1);
		const r = await this.#fileReq(FileOp.List, body);
		if (!r || r.status !== 0) return null;
		const entries: FileEntry[] = [];
		const p = r.rest;
		let i = 0;
		while (i < p.length) {
			const isDir = p[i++] === 1;
			const size = (p[i] | (p[i + 1] << 8) | (p[i + 2] << 16) | (p[i + 3] << 24)) >>> 0;
			i += 4;
			let j = i;
			while (j < p.length && p[j]) j++;
			const name = new TextDecoder().decode(p.subarray(i, j));
			i = j + 1;
			entries.push({ name, isDir, size });
		}
		return entries;
	}

	async readFile(path: string): Promise<Uint8Array | null> {
		const pb = new TextEncoder().encode(path);
		const body = new Uint8Array(1 + pb.length);
		body[0] = FileOp.Read;
		body.set(pb, 1);
		const r = await this.#fileReq(FileOp.Read, body);
		return r && r.status === 0 ? r.rest.slice() : null;
	}

	async writeFile(path: string, data: Uint8Array): Promise<boolean> {
		const pb = new TextEncoder().encode(path);
		const body = new Uint8Array(3 + pb.length + data.length);
		body[0] = FileOp.Write;
		body[1] = pb.length & 0xff;
		body[2] = (pb.length >> 8) & 0xff;
		body.set(pb, 3);
		body.set(data, 3 + pb.length);
		const r = await this.#fileReq(FileOp.Write, body);
		return !!r && r.status === 0;
	}

	// --- Firmware OTA (Plan 39): stream a .bin over the Ota channel ---
	#otaReq(frame: Uint8Array, timeoutMs = 15000): Promise<{ status: number; written: number; proto: number } | null> {
		return new Promise((resolve) => {
			const timer = setTimeout(() => {
				this.#otaPending = null;
				resolve(null); // no reply within the window
			}, timeoutMs);
			this.#otaPending = (r) => {
				clearTimeout(timer);
				resolve(r);
			};
			this.#t.send(encodeFrame(Channel.Ota, frame));
		});
	}

	// Push a firmware image: Begin(size) → Data chunks → End, each waiting for the
	// device ack (flow control + progress). onProgress drives the bar; onStatus is a
	// human log of each phase/error so the upload is debuggable on real hardware.
	// Transport-agnostic (USB / BLE / sim).
	async otaUpdate(
		image: Uint8Array,
		onProgress?: (sent: number, total: number) => void,
		onStatus?: (msg: string) => void
	): Promise<boolean> {
		const log = (m: string) => onStatus?.(m);
		const OtaOp = { Begin: 0x01, Data: 0x02, End: 0x03 };
		const CHUNK = 2048; // 1024 was safe; 4096 broke HW (USB-CDC RX ring overflow); 2048 is the middle ground

		const begin = new Uint8Array(5);
		begin[0] = OtaOp.Begin;
		begin[1] = image.length & 0xff;
		begin[2] = (image.length >> 8) & 0xff;
		begin[3] = (image.length >> 16) & 0xff;
		begin[4] = (image.length >>> 24) & 0xff;
		// esp_ota_begin erases ~image-size of flash up front — seconds on a real
		// device, and it stalls the UI meanwhile. Give it a long window.
		log(`preparing — device is erasing ~${(image.length / 1024).toFixed(0)} KB of flash (a few seconds; the screen may freeze)…`);
		let r = await this.#otaReq(begin, 120000);
		if (!r) {
			log('no response. The device is offline, or its firmware has no OTA — flash the OTA build over cable once first.');
			return false;
		}
		if (r.status === 2) {
			log('unsupported: this firmware has no A/B OTA slots. Flash the OTA build over cable once, then retry.');
			return false;
		}
		if (r.status !== 0) {
			log('begin failed — could not open the update slot.');
			return false;
		}
		// Catch a stale device firmware (old OTA wire format) up front, instead of
		// failing mysteriously at 0% when esp_ota_write rejects a mis-framed chunk.
		const OTA_PROTO = 2;
		if (r.proto !== OTA_PROTO) {
			log(
				`firmware OTA protocol mismatch: device v${r.proto || '≤1'}, Forge v${OTA_PROTO}. ` +
					'Re-flash the device over cable with the latest build (bun run dev:ota skyrizz-e32 → idf.py flash), then reload Forge.'
			);
			return false;
		}

		log(`uploading ${(image.length / 1024).toFixed(0)} KB…`);
		const RETRIES = 8;
		let off = 0;
		while (off < image.length) {
			const chunk = image.subarray(off, off + CHUNK);
			// Data frame: [op][offset:4 LE][bytes]. The offset lets the device dedupe
			// a resent chunk (idempotent), so we can safely retry on a dropped frame
			// or a lost ack instead of aborting the whole multi-minute upload.
			const frame = new Uint8Array(5 + chunk.length);
			frame[0] = OtaOp.Data;
			frame[1] = off & 0xff;
			frame[2] = (off >> 8) & 0xff;
			frame[3] = (off >> 16) & 0xff;
			frame[4] = (off >>> 24) & 0xff;
			frame.set(chunk, 5);
			const pct = Math.round((off / image.length) * 100);

			let acked: { status: number; written: number; proto: number } | null = null;
			for (let attempt = 0; attempt < RETRIES && !acked; attempt++) {
				if (attempt > 0) {
					log(`retrying at ${pct}% (attempt ${attempt + 1}/${RETRIES})…`);
					await new Promise((res) => setTimeout(res, 150));
				}
				const rr = await this.#otaReq(frame, 20000);
				if (rr && rr.status === 0) acked = rr;
				else if (rr && rr.status === 2) {
					log('unsupported mid-stream — aborting.');
					return false;
				}
				// rr null (timeout / dropped) or status 1 (transient) → retry the same chunk
			}
			if (!acked) {
				log(`gave up at ${pct}% after ${RETRIES} retries — upload aborted.`);
				return false;
			}
			// Advance to the device's authoritative written count (handles dedup).
			off = Math.max(off + chunk.length, acked.written);
			onProgress?.(off, image.length);
		}

		log('finalizing — verifying the image…');
		r = await this.#otaReq(new Uint8Array([OtaOp.End]), 30000);
		if (r && r.status === 0) {
			log('done — device rebooting into the new image. Run `version` to confirm.');
			return true;
		}
		if (!r) {
			// All bytes were written + acked; a missing End ack usually just means
			// the device already rebooted before the reply got out.
			log('upload complete; no final ack (device likely rebooting). Run `version` to confirm.');
			return true;
		}
		log('finalize failed — the device rejected the image (bad/corrupt).');
		return false;
	}

	async #filePathOp(op: number, path: string): Promise<boolean> {
		const pb = new TextEncoder().encode(path);
		const body = new Uint8Array(1 + pb.length);
		body[0] = op;
		body.set(pb, 1);
		const r = await this.#fileReq(op, body);
		return !!r && r.status === 0;
	}
	mkdir(path: string) {
		return this.#filePathOp(FileOp.Mkdir, path);
	}
	removeFile(path: string) {
		return this.#filePathOp(FileOp.Remove, path);
	}
	power(op: number) {
		this.#t.send(encodeFrame(Channel.System, new Uint8Array([op])));
	}
	injectEvent(name: string) {
		const enc = new TextEncoder().encode(name);
		const p = new Uint8Array(1 + enc.length + 1);
		p[0] = ExtOp.InjectEvent;
		p.set(enc, 1);
		this.#t.send(encodeFrame(Channel.Ext, p));
	}
	wifiSetNetworks(nets: { ssid: string; password: string; rssi: number; online: boolean }[]) {
		const text = nets
			.map((n) => `${n.ssid}\t${n.password}\t${n.rssi}\t${n.online ? 1 : 0}`)
			.join('\n');
		const enc = new TextEncoder().encode(text);
		const p = new Uint8Array(1 + enc.length);
		p[0] = ExtOp.WifiSetNetworks;
		p.set(enc, 1);
		this.#t.send(encodeFrame(Channel.Ext, p));
	}

	// OTA-install a custom app: send a built `.kapp` (text) → device JsAppStore
	// installs it live (volatile; appears in Apps immediately). Plan 37 Fase 6.
	installApp(kapp: string) {
		const body = new TextEncoder().encode(kapp);
		const p = new Uint8Array(1 + body.length);
		p[0] = ExtOp.AppInstall;
		p.set(body, 1);
		this.#t.send(encodeFrame(Channel.Ext, p));
	}
}
