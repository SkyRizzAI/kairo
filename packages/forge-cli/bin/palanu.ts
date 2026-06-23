#!/usr/bin/env bun
// palanu — Forge CLI for Palanu devices.
// Remote device management, shell, and file copy over USB serial or WebSocket.

import { Command } from "commander";
import { addCommand } from "../src/commands/add.js";
import { listCommand } from "../src/commands/list.js";
import { connectCommand } from "../src/commands/connect.js";
import { disconnectCommand } from "../src/commands/disconnect.js";
import { removeCommand } from "../src/commands/remove.js";
import { shellCommand } from "../src/commands/shell.js";
import { cpCommand } from "../src/commands/cp.js";
import { lsCommand } from "../src/commands/ls.js";
import { mkdirCommand } from "../src/commands/mkdir.js";
import { execCommand } from "../src/commands/exec.js";

// Shared option: device password used as a challenge-response fallback when the
// stored token is rejected (e.g. after the device rebooted and invalidated it).
const PW = ["-p, --password <password>", "device password (fallback if the saved token is rejected)"] as const;

const program = new Command();

program
	.name("palanu")
	.description("Forge CLI — remote device management for Palanu")
	.version("0.1.0");

program
	.command("add <name> <target>")
	.description("register a device (alias → target URL). e.g. palanu add mydev serial:///dev/cu.usbmodem123")
	.action(async (name: string, target: string) => {
		try {
			await addCommand(name, target);
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program
	.command("list")
	.description("list registered devices with connection status")
	.action(async () => {
		try {
			await listCommand();
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program
	.command("connect <name>")
	.description("connect to a device by alias")
	.option(...PW)
	.action(async (name: string, options: { password?: string }) => {
		try {
			await connectCommand(name, options.password);
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program
	.command("disconnect <name>")
	.description("disconnect from a device")
	.action(async (name: string) => {
		try {
			await disconnectCommand(name);
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program
	.command("remove <name>")
	.description("remove a device from the registry")
	.action(async (name: string) => {
		try {
			await removeCommand(name);
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program
	.command("shell <name>")
	.description("interactive remote CLI (REPL) — like SSH")
	.option(...PW)
	.action(async (name: string, options: { password?: string }) => {
		try {
			await shellCommand(name, options.password);
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program
	.command("exec <name> <command...>")
	.description("run one CLI command on a device and print its output (non-interactive)")
	.option(...PW)
	.action(async (name: string, commandParts: string[], options: { password?: string }) => {
		try {
			await execCommand(name, commandParts.join(" "), { password: options.password });
			process.exit(0);
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program
	.command("cp <src> <dst>")
	.description("copy file device↔local (like scp). Use device:<name>:/path for remote. e.g. palanu cp device:mydev:/file ./local/")
	.option(...PW)
	.action(async (src: string, dst: string, options: { password?: string }) => {
		try {
			await cpCommand(src, dst, options.password);
			process.exit(0);
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program
	.command("ls <path>")
	.description("list files on a device. Path: device:<name>:/path e.g. palanu ls device:usb:/sd")
	.option(...PW)
	.action(async (path: string, options: { password?: string }) => {
		try {
			await lsCommand(path, options.password);
			process.exit(0);
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program
	.command("mkdir <path>")
	.description("create a directory on a device. Path: device:<name>:/path")
	.option(...PW)
	.action(async (path: string, options: { password?: string }) => {
		try {
			await mkdirCommand(path, options.password);
			process.exit(0);
		} catch (e) {
			console.error(`✗ ${(e as Error).message}`);
			process.exit(1);
		}
	});

program.parse();
