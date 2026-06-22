import { VirtualCableTransport } from './transport/VirtualCableTransport';
import { RemoteSession } from '@palanu/link';

// One shared WASM simulator instance for the whole site. /simulator and /remote
// attach to the SAME running firmware (the basis for "remote the simulator").
let session: RemoteSession | null = null;

export function wasmSession(): RemoteSession {
	if (!session) session = new RemoteSession(new VirtualCableTransport());
	return session;
}
