// palanu exec <name> <command...> — run a single CLI command on the device and
// print its output, non-interactively (like `ssh host cmd`). Exits when the device
// signals end-of-output (EOT) for the command, or on timeout.

import { createSession } from "../session.js";

export async function execCommand(
	name: string,
	command: string,
	opts: { password?: string; timeoutMs?: number } = {}
): Promise<void> {
	const session = await createSession(name, { password: opts.password });

	const SID = 0;
	const timeoutMs = opts.timeoutMs ?? 15000;

	await new Promise<void>((resolve, reject) => {
		let settled = false;
		const finish = (err?: Error) => {
			if (settled) return;
			settled = true;
			clearTimeout(timer);
			off();
			offDc();
			session.close();
			err ? reject(err) : resolve();
		};
		const timer = setTimeout(
			() => finish(new Error(`exec timed out after ${timeoutMs / 1000}s with no end-of-output`)),
			timeoutMs
		);
		const offDc = session.onDisconnect(() => finish(new Error("device disconnected mid-command")));
		const off = session.on("cli", (chunk) => {
			if (chunk.sid !== SID) return;
			if (chunk.text) process.stdout.write(chunk.text + "\n");
			// `prompt` updates carry the cwd; ignore for one-shot exec.
			if (chunk.done) finish();
		});
		session.sendCli(SID, command);
	});
}
