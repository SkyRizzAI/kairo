import type { RemoteSession } from '@palanu/link';

// The active remote session — module-level so it survives SvelteKit page
// navigation. Without this, leaving /remote orphaned the connected session: its
// Web Serial read loop kept the OS port locked, the page state reset to the
// picker, and reconnecting failed with "port already open".
//
// `owned` marks sessions the remote page created (BLE/USB) and may close on
// disconnect. The WASM simulator session is shared site-wide (wasmSession())
// and must NOT be closed here — disconnect just detaches from it.

interface ActiveRemote {
	session: RemoteSession;
	label: string;
	owned: boolean;
}

let current: ActiveRemote | null = null;

export function activeRemote(): ActiveRemote | null {
	return current;
}

export function setRemote(session: RemoteSession, label: string, owned = true) {
	// Close the previous owned session before replacing it, or its transport (e.g. a
	// Web Serial port) stays OS-locked and its read loop keeps running (F4).
	if (current?.owned && current.session !== session) current.session.close();
	current = { session, label, owned };
}

export function clearRemote() {
	if (current?.owned) current.session.close();
	current = null;
}
