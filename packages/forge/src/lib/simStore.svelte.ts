import { wasmSession } from './wasmSim';
import type { ScreenFrame, LogEntry, EventEntry, BoardProfile, CliChunk } from './RemoteSession';

// Reactive WASM simulator store — wraps the shared wasmSession() and exposes the
// firmware's telemetry (screen / logs / events / services) + control commands.
// Same data shape the old native-bridge store had, now over KLP. The rich
// /simulator UI binds to this.
class SimStore {
	frame = $state<ScreenFrame | null>(null);
	profile = $state<BoardProfile | null>(null);
	logs = $state<LogEntry[]>([]);
	events = $state<EventEntry[]>([]);
	services = $state<Record<string, string>>({});
	connected = $state(false);
	// Power state — the device starts OFF. The WASM firmware is NOT loaded until
	// boot() is called (no auto-boot on page open). 'booting' covers WASM fetch +
	// pthread pool spin-up + KLP handshake; flips to 'on' when connected.
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
		this.#s.on('log', (l) => (this.logs = [...this.logs.slice(-300), l]));
		this.#s.on('event', (e) => {
			this.events = [...this.events.slice(-300), e];
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
	// CLI terminal passthrough (same KLP channel as /remote).
	sendCli(sid: number, line: string) {
		this.#s.sendCli(sid, line);
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
}

export const simStore = new SimStore();
