import { Channel, Flags, encodeFrame, FrameParser, rleDecode } from '$lib/klp/codec';
import type { ILinkTransport } from '$lib/klp/transport';

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

const HELLO = 0x01;
const ACK = 0x02;

export const Power = { Restart: 0x10, Sleep: 0x11, Shutdown: 0x12 } as const;
export const Key = { Up: 1, Down: 2, Left: 3, Right: 4, Select: 5, Cancel: 6 } as const;
const ExtOp = { InjectEvent: 0x01, WifiSetNetworks: 0x02, AppInstall: 0x03 } as const;

type Listeners = {
	screen: Set<(f: ScreenFrame) => void>;
	log: Set<(l: LogEntry) => void>;
	event: Set<(e: EventEntry) => void>;
	ready: Set<() => void>;
};

// RemoteSession — transport-agnostic client of a Kairo device (WASM sim or real
// hardware) over KLP. Multi-listener so /simulator and /remote can both observe
// the SAME device without clobbering each other.
export class RemoteSession {
	#t: ILinkTransport;
	#parser = new FrameParser();
	#ready = false;
	#helloTimer: ReturnType<typeof setInterval> | null = null;
	#l: Listeners = { screen: new Set(), log: new Set(), event: new Set(), ready: new Set() };

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

	// Power on the cable (load/boot the WASM firmware, or open BLE/USB). The
	// onState callback drives the handshake once the transport reports connected.
	boot(): void | Promise<void> {
		return this.#t.boot?.();
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
			}
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
