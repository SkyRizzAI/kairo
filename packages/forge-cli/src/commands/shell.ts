// palanu shell <name> — interactive remote CLI REPL (like SSH).
// Opens a RemoteSession, uses PLP Cli channel (sid=0), displays prompt cwd$,
// streams output realtime. Handles auth handshake (initial + re-auth).

import * as readline from "node:readline";
import { setActiveSession } from "../registry.js";
import { createSession } from "../session.js";

type ShellState = "connecting" | "auth" | "shell" | "closing";

export async function shellCommand(name: string, password?: string): Promise<void> {
	console.log(`connecting to "${name}"…`);
	const session = await createSession(name, { password });
	setActiveSession(name, session);

	const SID = 0;
	let cwd = "/";
	let state: ShellState = "connecting";
	let started = false;

	const rl = readline.createInterface({
		input: process.stdin,
		output: process.stdout,
		terminal: false,
	});

	function showPrompt() {
		rl.setPrompt(`${cwd}$ `);
		rl.prompt();
	}

	function promptPassword(msg: string) {
		state = "auth";
		rl.setPrompt("");
		rl.question(msg, async (pw) => {
			await session.submitPassword(pw);
		});
	}

	function startShell() {
		if (started) {
			// Resuming after re-auth
			state = "shell";
			showPrompt();
			return;
		}
		started = true;
		state = "shell";
		console.log(`\nConnected to "${name}". Type 'exit' to leave.\n`);
		showPrompt();
	}

	// ── Auth events ──
	session.on("auth", () => {
		if (state === "auth") return; // already prompting
		promptPassword("password: ");
	});

	session.on("authorized", () => {
		if (state === "auth" || !started) {
			startShell();
		}
	});

	session.on("authfail", () => {
		console.log("auth failed");
		promptPassword("password: ");
	});

	session.on("rejected", () => {
		console.error("device rejected connection — remote is disabled");
		session.close();
		process.exit(1);
	});

	// ── CLI output ──
	// Device sends each `out()` call as a separate frame without \n.
	// Forge web renders each as a <div> (CSS gives newline). CLI needs explicit \n.
	session.on("cli", (chunk) => {
		if (state !== "shell") return;
		if (chunk.text) {
			process.stdout.write(chunk.text + "\n");
		}
		if (chunk.prompt) cwd = chunk.prompt;
		if (chunk.done) {
			showPrompt();
		}
	});

	// ── Input ──
	rl.on("line", (line) => {
		if (state === "auth") {
			// During auth, readline auto-handles via rl.question callback.
			// This line callback shouldn't fire during question() but just in case:
			return;
		}
		if (state !== "shell") return;

		const cmd = line.trim();
		if (cmd === "exit" || cmd === "quit") {
			state = "closing";
			session.close();
			rl.close();
			return;
		}
		if (cmd === "") {
			showPrompt();
			return;
		}
		session.sendCli(SID, cmd);
	});

	rl.on("close", () => {
		session.close();
		process.exit(0);
	});

	// If already authorized (token accepted during createSession), start shell
	if (session.authorized) {
		startShell();
	}
}
