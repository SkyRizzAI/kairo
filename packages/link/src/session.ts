// RemoteSession — transport-agnostic client of a Palanu device (WASM sim or real
// hardware) over PLP. Multi-listener so /simulator and /remote can both observe
// the SAME device without clobbering each other.
//
// Plan 77: extracted from packages/forge/src/lib/RemoteSession.ts.
// Refactored: localStorage → ITokenStore (injected via constructor) so this
// module is isomorphic (works in both browser and Node).

import { Channel, Flags, encodeFrame, FrameParser, rleDecode } from './codec';
import type { ILinkTransport } from './transport';
import type {
	ScreenFrame,
	LogEntry,
	EventEntry,
	CliChunk,
	FileEntry,
	BoardProfile,
	BoardComponent
} from './types';
import { Power, Key, KEY_MAP, frameDims } from './types';
import type { ITokenStore } from './tokens';
import { LocalStorageTokenStore, DEFAULT_TOKEN_KEY } from './tokens';

// Re-export so consumers can import everything from @palanu/link/session.
export type { ScreenFrame, LogEntry, EventEntry, CliChunk, FileEntry, BoardProfile, BoardComponent };
export { Power, Key, KEY_MAP, frameDims };

const HELLO = 0x01;
const ACK = 0x02;
const REJECT = 0x03; // device refused the handshake (Remote disabled)
const PING = 0x10;
const PONG = 0x11;
// Session auth (Plan 74) — Control channel opcodes.
const AUTH_CHALLENGE = 0x20;
const AUTH_RESPONSE = 0x21;
const AUTH_OK = 0x22;
const AUTH_FAIL = 0x23;
const AUTH_REQUIRED = 0x24;
const GET_INFO = 0x01; // SYSTEM channel opcode

// SHA-256 hex via Web Crypto (browser + Node). Mirrors the device's hexSha256.
async function sha256hex(s: string): Promise<string> {
	const buf = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(s));
	return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

const ExtOp = { InjectEvent: 0x01, WifiSetNetworks: 0x02, AppInstall: 0x03, AppScan: 0x04 } as const;
// FILE v2 (Plan 88): every request/reply carries a 2-byte reqId after the opcode,
// so replies correlate by id (Map) instead of FIFO-per-opcode. Writes are chunked
// (Begin→Data×N→End), paced by the device's per-chunk ack, so a large file never
// overruns the device USB-CDC RX ring (the `cp` failure root cause).
const FileOp = {
	List: 0x01,
	Read: 0x03,
	Mkdir: 0x05,
	Remove: 0x06,
	Rename: 0x07,
	Copy: 0x08,
	WriteBegin: 0x10,
	WriteData: 0x11,
	WriteEnd: 0x12,
	ReadBegin: 0x13,
	ReadData: 0x14,
	ReadEnd: 0x15
} as const;

// Host-side default write chunk; the device may dictate a smaller one in the
// WriteBegin ack. Kept under the OTA-proven 1792 B USB-CDC RX ceiling.
const FILE_WRITE_CHUNK = 1024;

// Structured FILE reply codes (mirror firmware FileStatus, Plan 88 R6).
const FILE_STATUS_NAME: Record<number, string> = {
	0: 'ok',
	1: 'not found',
	2: 'error',
	3: 'no space left on device',
	4: 'too many open files',
	5: 'permission denied / read-only',
	6: 'I/O error',
	7: 'bad request'
};
export function fileStatusName(code: number): string {
	return FILE_STATUS_NAME[code] ?? `status ${code}`;
}

type Listeners = {
	screen: Set<(f: ScreenFrame) => void>;
	log: Set<(l: LogEntry) => void>;
	event: Set<(e: EventEntry) => void>;
	ready: Set<() => void>;
	profile: Set<(p: BoardProfile) => void>;
	cli: Set<(c: CliChunk) => void>;
	// Plan 74: device wants a password ('auth'), or session was authorized.
	auth: Set<() => void>;
	authorized: Set<() => void>;
	authfail: Set<() => void>;
	rejected: Set<() => void>; // Remote disabled on the device
};

export interface RemoteSessionOptions {
	tokenStore?: ITokenStore;
	tokenKey?: string;
}

export class RemoteSession {
	#t: ILinkTransport;
	#parser = new FrameParser();
	#ready = false;
	#profile: BoardProfile | null = null;
	#helloTimer: ReturnType<typeof setInterval> | null = null;
	// In-protocol heartbeat (Plan 88 §8): ping the device while connected and tear
	// the session down if it goes silent (USB/serial gives no disconnect event).
	#pingTimer: ReturnType<typeof setInterval> | null = null;
	#lastRx = 0;
	#disconnected: Set<() => void> = new Set();
	// FILE channel is request/response, correlated by a monotonic reqId (FILE v2).
	// A reply for an unknown/expired reqId is ignored instead of being mis-matched
	// to another in-flight request (the v1 FIFO-per-opcode desync bug).
	#filePending = new Map<number, (r: { status: number; rest: Uint8Array } | null) => void>();
	// Chunked-read collectors (device pushes ReadBegin-ack → ReadData×N → ReadEnd for
	// one reqId). Keyed by reqId; the handler routes the streamed frames here.
	#readPending = new Map<number, (op: number, payload: Uint8Array) => void>();
	#nextReqId = 1; // 0 reserved / skipped on wrap
	#lastFileStatus = 0; // structured code of the most recent failed FILE op (R6)
	// OTA is strictly sequential (Begin→Data×N→End), so one in-flight resolver.
	#otaPending: ((r: { status: number; written: number; proto: number } | null) => void) | null = null;
	#l: Listeners = {
		screen: new Set(),
		log: new Set(),
		event: new Set(),
		ready: new Set(),
		profile: new Set(),
		cli: new Set(),
		auth: new Set(),
		authorized: new Set(),
		authfail: new Set(),
		rejected: new Set()
	};
	#pendingChallenge: { salt: string; nonce: string } | null = null;
	#authorized = false;
	#tokens: ITokenStore;
	#tokenKey: string;

	constructor(t: ILinkTransport, opts: RemoteSessionOptions = {}) {
		this.#t = t;
		this.#tokens = opts.tokenStore ?? new LocalStorageTokenStore();
		this.#tokenKey = opts.tokenKey ?? DEFAULT_TOKEN_KEY;
		t.onData((d) => this.#onBytes(d));
		t.onState((c) => {
			if (c) {
				this.#hello();
			} else {
				// Transport closed: drain pending requests + stop timers instead of
				// letting them hang until their individual timers fire (N4/§8).
				this.#drain();
			}
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
		if (this.#pingTimer) {
			clearInterval(this.#pingTimer);
			this.#pingTimer = null;
		}
		this.#ready = false;
		this.#t.close();
	}

	on<K extends keyof Listeners>(kind: K, fn: Listeners[K] extends Set<infer F> ? F : never) {
		(this.#l[kind] as Set<unknown>).add(fn as unknown);
		if (kind === 'screen') this.#syncScreen();
		return () => {
			(this.#l[kind] as Set<unknown>).delete(fn as unknown);
			if (kind === 'screen') this.#syncScreen();
		};
	}

	// The device's screen mirror is opt-in (Plan 88): it is a heavy continuous
	// stream that would otherwise starve the inbound file/CLI path on the device's
	// single USB RX task. We subscribe only while something is actually listening
	// for 'screen' frames — so Forge web (which renders the display) gets the mirror
	// automatically, and the file/CLI tooling never triggers the flood. Re-sent on
	// every handshake since the device resets the flag per session.
	#syncScreen() {
		if (!this.#ready) return;
		const want = this.#l.screen.size > 0;
		// SYSTEM: [ScreenStream=0x02][on]
		this.#t.send(encodeFrame(Channel.System, new Uint8Array([0x02, want ? 1 : 0])));
	}

	// Explicit override, if a consumer wants the mirror without holding a listener.
	enableScreen(on = true) {
		this.#t.send(encodeFrame(Channel.System, new Uint8Array([0x02, on ? 1 : 0])));
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

	get authorized() {
		return this.#authorized;
	}

	// True when the device issued a password challenge that hasn't been satisfied —
	// i.e. the device is password-protected and we're not authorized yet. Lets the
	// host distinguish "open device, no auth needed" from "locked, needs a password".
	get authRequired() {
		return this.#pendingChallenge !== null && !this.#authorized;
	}

	// Called by the UI in response to the 'auth' event (device wants a password).
	async submitPassword(pw: string) {
		if (!this.#pendingChallenge) return;
		const { salt, nonce } = this.#pendingChallenge;
		const pwhash = await sha256hex(salt + pw);
		const response = await sha256hex(pwhash + nonce);
		this.#sendAuthResponse('H', response);
	}

	#sendAuthResponse(kind: 'H' | 'T', value: string) {
		const enc = new TextEncoder().encode(value);
		const p = new Uint8Array(2 + enc.length);
		p[0] = AUTH_RESPONSE;
		p[1] = kind.charCodeAt(0);
		p.set(enc, 2);
		this.#t.send(encodeFrame(Channel.Control, p));
	}

	#savedToken(): string | null {
		return this.#tokens.get(this.#tokenKey);
	}
	#saveToken(t: string) {
		this.#tokens.set(this.#tokenKey, t);
	}
	#clearToken() {
		this.#tokens.remove(this.#tokenKey);
	}

	#onBytes(d: Uint8Array) {
		this.#lastRx = Date.now(); // any inbound traffic = device alive (heartbeat)
		for (const f of this.#parser.push(d)) this.#handle(f);
	}

	// Start pinging + watch for device silence once the handshake completes.
	#startHeartbeat() {
		if (this.#pingTimer) return;
		this.#lastRx = Date.now();
		const PING_MS = 3000;
		const DEAD_MS = 30000; // generous: an embedded device doing inline I/O can be briefly busy
		this.#pingTimer = setInterval(() => {
			if (!this.#ready) return;
			if (Date.now() - this.#lastRx > DEAD_MS) {
				this.#drain(); // device went silent → treat as disconnected
				return;
			}
			this.#t.send(encodeFrame(Channel.Control, new Uint8Array([PING])));
		}, PING_MS);
	}

	// Tear down session state (involuntary disconnect or device-silent timeout).
	// Idempotent; safe to call from onState(false) and the heartbeat watchdog.
	#drain() {
		const wasReady = this.#ready;
		this.#ready = false;
		this.#authorized = false;
		this.#pendingChallenge = null;
		if (this.#helloTimer) { clearInterval(this.#helloTimer); this.#helloTimer = null; }
		if (this.#pingTimer) { clearInterval(this.#pingTimer); this.#pingTimer = null; }
		const pending = this.#filePending;
		this.#filePending = new Map();
		for (const cb of pending.values()) cb(null);
		// Fail any in-flight chunked reads (ReadEnd with a failure marker → resolve null).
		const reads = this.#readPending;
		this.#readPending = new Map();
		for (const cb of reads.values()) cb(FileOp.ReadEnd, new Uint8Array([1]));
		if (this.#otaPending) { this.#otaPending(null); this.#otaPending = null; }
		if (wasReady) for (const fn of this.#disconnected) fn();
	}

	// Register a callback fired when the session drops (transport close or device
	// silence). Returns an unsubscribe fn.
	onDisconnect(fn: () => void) {
		this.#disconnected.add(fn);
		return () => this.#disconnected.delete(fn);
	}

	#handle(f: { channel: number; flags: number; payload: Uint8Array }) {
		if (f.channel === Channel.Control) {
			if (f.payload[0] === ACK) {
				this.#ready = true;
				this.#emit('ready');
				this.#startHeartbeat(); // begin pinging / device-silence watchdog
				// Handshake done → ask for the board profile (SYSTEM GetInfo).
				this.#t.send(encodeFrame(Channel.System, new Uint8Array([GET_INFO])));
				// Re-assert screen subscription (device resets it per session).
				this.#syncScreen();
			} else if (f.payload[0] === PONG) {
				/* heartbeat reply — #lastRx already bumped in #onBytes */
			} else if (f.payload[0] === REJECT) {
				// Remote is disabled on the device — stop retrying the handshake.
				if (this.#helloTimer) {
					clearInterval(this.#helloTimer);
					this.#helloTimer = null;
				}
				this.#emit('rejected');
			} else if (f.payload[0] === AUTH_CHALLENGE) {
				// "salt:nonce" — split on the FIRST colon only and validate both halves,
				// so a malformed challenge fails loudly instead of silently mis-parsing (F8).
				const text = new TextDecoder().decode(f.payload.subarray(1));
				const sep = text.indexOf(':');
				if (sep < 0) return; // not a valid challenge — ignore
				const salt = text.slice(0, sep);
				const nonce = text.slice(sep + 1);
				if (!salt || !nonce) return;
				this.#pendingChallenge = { salt, nonce };
				const token = this.#savedToken();
				if (token) this.#sendAuthResponse('T', token);
				else this.#emit('auth');
			} else if (f.payload[0] === AUTH_OK) {
				this.#authorized = true;
				const tok = new TextDecoder().decode(f.payload.subarray(1));
				if (tok) this.#saveToken(tok);
				this.#emit('authorized');
				// Screen is gated by auth on the device — (re)assert once authorized.
				this.#syncScreen();
				// Re-request the board profile NOW that we're authorized: the GetInfo
				// sent on ACK is dropped by a password-protected device (System channel
				// is gated until auth), so on a locked device only this post-auth request
				// actually gets answered — otherwise profile stays null ("unknown board").
				this.#t.send(encodeFrame(Channel.System, new Uint8Array([GET_INFO])));
			} else if (f.payload[0] === AUTH_FAIL) {
				this.#clearToken(); // a stale token / wrong password — prompt fresh
				this.#emit('authfail');
				this.#emit('auth');
			} else if (f.payload[0] === AUTH_REQUIRED) {
				if (!this.#authorized) this.#emit('auth');
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
			const p = f.payload;
			if (p.length < 3) return;
			const op = p[0];
			const reqId = p[1] | (p[2] << 8);
			// Streaming read frames (ReadBegin-ack / ReadData / ReadEnd) go to the
			// per-reqId collector; everything else is a single [op][reqId][status][rest].
			const rc = this.#readPending.get(reqId);
			if (rc) {
				rc(op, p.subarray(3));
				return;
			}
			if (p.length < 4) return;
			const status = p[3];
			const rest = p.subarray(4);
			this.#filePending.get(reqId)?.({ status, rest });
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
			// Reject implausible dimensions before allocating w*h pixels.
			if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return;
			const expected = w * h;
			const body = p.subarray(4);
			const px = f.flags & Flags.Compressed ? rleDecode(body, expected) : body;
			// Drop a short/malformed frame instead of rendering garbage.
			if (px.length < expected) return;
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
	// Sends [op][reqId:2][payload]; resolves when the matching reqId reply arrives.
	#fileReq(op: number, payload: Uint8Array, timeoutMs = 5000): Promise<{ status: number; rest: Uint8Array } | null> {
		return new Promise((resolve) => {
			const reqId = this.#nextReqId;
			this.#nextReqId = ((this.#nextReqId + 1) & 0xffff) || 1; // wrap, skip 0
			const done = (r: { status: number; rest: Uint8Array } | null) => {
				clearTimeout(timer);
				this.#filePending.delete(reqId);
				resolve(r);
			};
			const timer = setTimeout(() => done(null), timeoutMs); // no reply → null
			this.#filePending.set(reqId, done);
			const body = new Uint8Array(3 + payload.length);
			body[0] = op;
			body[1] = reqId & 0xff;
			body[2] = (reqId >> 8) & 0xff;
			body.set(payload, 3);
			this.#t.send(encodeFrame(Channel.File, body));
		});
	}

	async listDir(path: string): Promise<FileEntry[] | null> {
		const r = await this.#fileReq(FileOp.List, new TextEncoder().encode(path));
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

	// Chunked read (FILE v2): ReadBegin → device pushes ReadData×N → ReadEnd. The
	// device→host direction has no RX-ring limit, so it streams without per-chunk
	// acks; the host reassembles by offset. Removes the old 64 KB single-frame cap.
	readFile(path: string): Promise<Uint8Array | null> {
		const pb = new TextEncoder().encode(path);
		return new Promise((resolve) => {
			const reqId = this.#nextReqId;
			this.#nextReqId = ((this.#nextReqId + 1) & 0xffff) || 1;
			let total = -1;
			let buf: Uint8Array | null = null;
			let received = 0;
			let settled = false;
			const done = (r: Uint8Array | null) => {
				if (settled) return;
				settled = true;
				clearTimeout(timer);
				this.#readPending.delete(reqId);
				resolve(r);
			};
			const timer = setTimeout(() => done(null), 30000);
			this.#readPending.set(reqId, (op, payload) => {
				if (op === FileOp.ReadBegin) {
					// [status][total:4][chunkSize:2]
					if (payload.length < 5 || payload[0] !== 0) { done(null); return; }
					total = (payload[1] | (payload[2] << 8) | (payload[3] << 16) | payload[4] * 0x1000000) >>> 0;
					buf = new Uint8Array(total);
				} else if (op === FileOp.ReadData) {
					if (!buf || payload.length < 4) return;
					const off = (payload[0] | (payload[1] << 8) | (payload[2] << 16) | payload[3] * 0x1000000) >>> 0;
					const bytes = payload.subarray(4);
					if (off + bytes.length <= buf.length) {
						buf.set(bytes, off);
						received += bytes.length;
					}
				} else if (op === FileOp.ReadEnd) {
					const status = payload.length >= 1 ? payload[0] : 1;
					done(status === 0 && buf && received >= total ? buf : null);
				}
			});
			const body = new Uint8Array(3 + pb.length);
			body[0] = FileOp.ReadBegin;
			body[1] = reqId & 0xff;
			body[2] = (reqId >> 8) & 0xff;
			body.set(pb, 3);
			this.#t.send(encodeFrame(Channel.File, body));
		});
	}

	// Chunked write (FILE v2): WriteBegin(totalSize,path) → WriteData(offset,bytes)×N
	// → WriteEnd. Each WriteData is paced by the device ack, so the burst never
	// overruns the device USB-CDC RX ring (a single whole-file frame did — the root
	// cause of the `cp` failure). Acks carry the device's next expected offset, so a
	// dropped/timed-out chunk is retried idempotently.
	async writeFile(path: string, data: Uint8Array): Promise<boolean> {
		const pb = new TextEncoder().encode(path);
		const RETRIES = 8;

		// Retry a request that times out (null). Every FILE op carries a fresh reqId
		// and the device handles each phase idempotently (re-Begin keeps progress,
		// WriteData dedups by offset, WriteEnd replays the stored result), so a lost
		// frame/ack is safe to resend. A reply with any status (ok or error) returns
		// to the caller, which decides.
		const reqWithRetry = async (
			op: number,
			body: Uint8Array,
			timeout: number
		): Promise<{ status: number; rest: Uint8Array } | null> => {
			for (let attempt = 0; attempt < RETRIES; attempt++) {
				if (attempt > 0) await new Promise((res) => setTimeout(res, 100));
				const rr = await this.#fileReq(op, body, timeout);
				if (rr) return rr;
			}
			return null;
		};

		// Begin: [totalSize:4 LE][path]
		const begin = new Uint8Array(4 + pb.length);
		begin[0] = data.length & 0xff;
		begin[1] = (data.length >> 8) & 0xff;
		begin[2] = (data.length >> 16) & 0xff;
		begin[3] = (data.length >>> 24) & 0xff;
		begin.set(pb, 4);
		const br = await reqWithRetry(FileOp.WriteBegin, begin, 15000);
		if (!br || br.status !== 0) {
			this.#lastFileStatus = br ? br.status : 2;
			return false;
		}

		// Begin ack rest = [chunkSize:2 LE][xferId:2 LE]. The xferId tags every
		// WriteData/WriteEnd so the device binds them to THIS transfer (separate from
		// the per-message reqId, which differs each frame).
		let chunk = FILE_WRITE_CHUNK;
		if (br.rest.length >= 2) {
			const dev = br.rest[0] | (br.rest[1] << 8);
			if (dev > 0) chunk = Math.min(chunk, dev);
		}
		const xferId = br.rest.length >= 4 ? br.rest[2] | (br.rest[3] << 8) : 0;
		const xidLo = xferId & 0xff,
			xidHi = (xferId >> 8) & 0xff;

		let off = 0;
		while (off < data.length) {
			const slice = data.subarray(off, off + chunk);
			// Data: [xferId:2][offset:4 LE][bytes]
			const frame = new Uint8Array(6 + slice.length);
			frame[0] = xidLo;
			frame[1] = xidHi;
			frame[2] = off & 0xff;
			frame[3] = (off >> 8) & 0xff;
			frame[4] = (off >> 16) & 0xff;
			frame[5] = (off >>> 24) & 0xff;
			frame.set(slice, 6);

			const rr = await reqWithRetry(FileOp.WriteData, frame, 10000);
			if (!rr || rr.status !== 0) {
				this.#lastFileStatus = rr ? rr.status : 2;
				return false; // timeout-exhausted or device error → abort
			}
			// This chunk is acked (status 0 → the device has [off, off+len)). Advance
			// deterministically by what we sent; retries resend the SAME offset until
			// acked, so this never races the device's running buffer length. (The ack's
			// `next` field is only needed for resume-after-reconnect, not linear send.)
			off += slice.length;
		}

		const er = await reqWithRetry(FileOp.WriteEnd, new Uint8Array([xidLo, xidHi]), 30000);
		if (!er || er.status !== 0) {
			this.#lastFileStatus = er ? er.status : 2;
			return false;
		}
		this.#lastFileStatus = 0;
		return true;
	}

	// Human-readable reason for the most recent failed writeFile (R6).
	get lastFileError(): string {
		return fileStatusName(this.#lastFileStatus);
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
		const CHUNK = 1792; // 1024 was safe; 4096 broke HW (USB-CDC RX ring overflow); 2048 confirmed ok

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
		const r = await this.#fileReq(op, new TextEncoder().encode(path));
		return !!r && r.status === 0;
	}
	// Two-path op payload: [srcLen:2 LE][src][dst]
	async #fileTwoPathOp(op: number, src: string, dst: string): Promise<boolean> {
		const sb = new TextEncoder().encode(src);
		const db = new TextEncoder().encode(dst);
		const payload = new Uint8Array(2 + sb.length + db.length);
		payload[0] = sb.length & 0xff;
		payload[1] = (sb.length >> 8) & 0xff;
		payload.set(sb, 2);
		payload.set(db, 2 + sb.length);
		const r = await this.#fileReq(op, payload);
		return !!r && r.status === 0;
	}
	mkdir(path: string) {
		return this.#filePathOp(FileOp.Mkdir, path);
	}
	removeFile(path: string) {
		return this.#filePathOp(FileOp.Remove, path);
	}
	async removeAll(path: string): Promise<boolean> {
		const children = await this.listDir(path);
		if (children) {
			const slash = path === '/';
			for (const c of children) {
				const child = slash ? '/' + c.name : path + '/' + c.name;
				await this.removeAll(child);
			}
		}
		return this.removeFile(path);
	}
	renameFile(src: string, dst: string) {
		return this.#fileTwoPathOp(FileOp.Rename, src, dst);
	}
	copyFile(src: string, dst: string) {
		return this.#fileTwoPathOp(FileOp.Copy, src, dst);
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

	// OTA-install a custom app: send a built `.papp` (text) → device JsAppStore
	// installs it live (volatile; appears in Apps immediately). Plan 37 Fase 6.
	installApp(papp: string) {
		const body = new TextEncoder().encode(papp);
		const p = new Uint8Array(1 + body.length);
		p[0] = ExtOp.AppInstall;
		p.set(body, 1);
		this.#t.send(encodeFrame(Channel.Ext, p));
	}

	// Trigger /system/apps/ rescan after writing .papp files to VFS. Plan 86 Fase 6.
	appScan() {
		const p = new Uint8Array(1);
		p[0] = ExtOp.AppScan;
		this.#t.send(encodeFrame(Channel.Ext, p));
	}
}
