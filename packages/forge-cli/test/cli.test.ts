// @ts-nocheck — validated at runtime by `bun test`.
import { test, expect } from "bun:test";
import { loopbackPair, RemoteSession, Channel, encodeFrame } from "@palanu/link";
import { parsePath } from "../src/session.js";

// Test path parsing for cp command
test("parsePath: device path", () => {
	const r = parsePath("device:mydev:/system/apps/app.papp");
	expect(r.device).toBe("mydev");
	expect(r.path).toBe("/system/apps/app.papp");
});

test("parsePath: local path", () => {
	const r = parsePath("./local/backup/file.txt");
	expect(r.device).toBeNull();
	expect(r.path).toBe("./local/backup/file.txt");
});

test("parsePath: local absolute path", () => {
	const r = parsePath("/home/user/file.txt");
	expect(r.device).toBeNull();
	expect(r.path).toBe("/home/user/file.txt");
});

// Test RemoteSession over loopback (simulates a device connection)
test("RemoteSession over loopback: handshake + cli", async () => {
	const [host, device] = loopbackPair("test");

	// Host side — the CLI would use this
	const hostSession = new RemoteSession(host);

	// Device side — simulate ACK + profile
	device.onData((d) => {
		// Parse the frame to see what the host sent
		const frames = new (class {
			buf = new Uint8Array(0);
			push(chunk: Uint8Array) {
				const merged = new Uint8Array(this.buf.length + chunk.length);
				merged.set(this.buf, 0);
				merged.set(chunk, this.buf.length);
				this.buf = merged;
				// Look for HELLO on Control channel
				if (this.buf.length >= 6 && this.buf[1] === 0x00 && this.buf[5] === 0x01) {
					// Send ACK
					const ack = encodeFrame(Channel.Control, new Uint8Array([0x02]));
					device.send(ack);
					// Send a profile
					const profile = JSON.stringify({ id: "test", name: "test-board", w: 60, h: 40, components: [] });
					const profFrame = encodeFrame(Channel.System, new Uint8Array([0x01, ...new TextEncoder().encode(profile)]));
					device.send(profFrame);
					this.buf = new Uint8Array(0);
				}
			}
		})();
		frames.push(d);
	});

	// Wait for ready
	await new Promise<void>((resolve) => {
		hostSession.on("ready", () => resolve());
	});

	expect(hostSession.ready).toBe(true);
	expect(hostSession.profile?.name).toBe("test-board");
});
