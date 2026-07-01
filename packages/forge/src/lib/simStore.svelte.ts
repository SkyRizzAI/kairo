import { wasmSession } from './wasmSim';
import type { ScreenFrame, LogEntry, EventEntry, BoardProfile, CliChunk, PaletteInfo } from '@palanu/link';

// Reactive WASM simulator store — wraps the shared wasmSession() and exposes the
// firmware's telemetry (screen / logs / events / services) + control commands.
// Same data shape the old native-bridge store had, now over PLP. The rich
// /simulator UI binds to this.
class SimStore {
	frame = $state<ScreenFrame | null>(null);
	// Device theme palette (Plan 92 Fase B) — the firmware drives the screen colours;
	// the sim mirror follows Settings → Appearances → Theme, not a web selection.
	palette = $state<PaletteInfo | null>(null);
	profile = $state<BoardProfile | null>(null);
	logs = $state<LogEntry[]>([]);
	events = $state<EventEntry[]>([]);
	services = $state<Record<string, string>>({});
	led = $state<string | null>(null);   // last LED colour "r,g,b" (from LedChanged)
	connected = $state(false);
	// Power state — the device starts OFF. The WASM firmware is NOT loaded until
	// boot() is called (no auto-boot on page open). 'booting' covers WASM fetch +
	// pthread pool spin-up + PLP handshake; flips to 'on' when connected.
	power = $state<'off' | 'booting' | 'on'>('off');

	#wired = false;
	#s = wasmSession();

	// Wire telemetry listeners (idempotent). Does NOT boot — call boot() for that.
	init() {
		if (this.#wired) return;
		this.#wired = true;
		this.connected = this.#s.ready;
		if (this.#s.ready) this.power = 'on';
		this.profile = this.#s.profile;
		this.#s.on('ready', () => {
			this.connected = true;
			this.power = 'on';
		});
		this.#s.on('profile', (p) => (this.profile = p));
		this.#s.on('screen', (f) => (this.frame = f));
		this.#s.on('palette', (p) => (this.palette = p));
		this.#s.on('log', (l) => (this.logs = [...this.logs.slice(-300), l]));
		this.#s.on('event', (e) => {
			this.events = [...this.events.slice(-300), e];
			if (e.name === 'LedChanged' && e.fields.rgb) this.led = e.fields.rgb;
			// Derive a service list from Service* events (best-effort).
			if (e.name.startsWith('Service')) {
				const n = e.fields.name || e.fields.service || e.fields.svc;
				if (n) this.services = { ...this.services, [n]: e.name.replace('Service', '') };
			}
		});
	}

	// Power on the device: load + boot the WASM firmware. Idempotent.
	boot() {
		this.init();
		if (this.power !== 'off') return;
		this.power = 'booting';
		void this.#s.boot();
	}

	sendKey(k: number) {
		this.#s.sendKey(k);
	}
	sendPower(op: number) {
		this.#s.power(op);
	}
	injectEvent(name: string) {
		this.#s.injectEvent(name);
	}
	wifiSetNetworks(nets: { ssid: string; password: string; rssi: number; online: boolean }[]) {
		this.#s.wifiSetNetworks(nets);
	}
	appScan() {
		this.#s.appScan();
	}
	// CLI terminal passthrough (same PLP channel as /remote).
	sendCli(sid: number, line: string) {
		this.#s.sendCli(sid, line);
	}
	// Firmware OTA passthrough — dry-run in the sim (SimOtaUpdater), real flow API.
	otaUpdate(
		image: Uint8Array,
		onProgress: (sent: number, total: number) => void,
		onStatus: (msg: string) => void
	) {
		return this.#s.otaUpdate(image, onProgress, onStatus);
	}
	onCli(fn: (c: CliChunk) => void) {
		return this.#s.on('cli', fn);
	}
	// FILE channel passthrough (virtual filesystem).
	listDir(path: string) {
		return this.#s.listDir(path);
	}
	readFile(path: string) {
		return this.#s.readFile(path);
	}
	writeFile(path: string, data: Uint8Array) {
		return this.#s.writeFile(path, data);
	}
	mkdir(path: string) {
		return this.#s.mkdir(path);
	}
	removeFile(path: string) {
		return this.#s.removeFile(path);
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
		return this.#s.renameFile(src, dst);
	}
	copyFile(src: string, dst: string) {
		return this.#s.copyFile(src, dst);
	}
}

export const simStore = new SimStore();
