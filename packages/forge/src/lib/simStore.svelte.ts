import { wasmSession } from './wasmSim';
import type { ScreenFrame, LogEntry, EventEntry } from './RemoteSession';

// Reactive WASM simulator store — wraps the shared wasmSession() and exposes the
// firmware's telemetry (screen / logs / events / services) + control commands.
// Same data shape the old native-bridge store had, now over KLP. The rich
// /simulator UI binds to this.
class SimStore {
	frame = $state<ScreenFrame | null>(null);
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
		this.#s.on('ready', () => {
			this.connected = true;
			this.power = 'on';
		});
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

	// Backwards-compatible alias used by /remote: ensure listeners are wired and
	// the device is powered on when a session attaches.
	connect() {
		this.boot();
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
}

export const simStore = new SimStore();
