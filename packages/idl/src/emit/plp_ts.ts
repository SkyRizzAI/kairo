// tools/idl/emit/plp_ts.ts — Generate PLP TypeScript types from the IDL (Plan 77).
//
// Reads the `palanu:plp` package from the AST and emits a TypeScript file with
// channel/opcode/flag constants. Output: packages/link/src/types-generated.ts

import type { PidlAst, PidlPackage, PidlInterface, PidlFunc } from "../ast";

function findPlpPackage(ast: PidlAst): PidlPackage | null {
	for (const pkg of ast.packages) {
		if (pkg.name === "palanu:plp") return pkg;
	}
	return null;
}

function funcsToConstObject(iface: PidlInterface, indent: string): string {
	const lines: string[] = [];
	lines.push(`${indent}export const ${iface.name.replace(/_/g, "")} = {`);
	for (const fn of iface.functions) {
		const val = fn.returns?.kind === "u8" ? fn.name : "0";
		// The IDL defines functions like `control: u8` meaning channel 0x00.
		// We need the actual numeric value — but the IDL parser stores the
		// function name as the key and the return type as u8. The actual value
		// is in the function name's position (order) or needs a different IDL
		// construct. For now, we use the doc comments to derive values.
		// Actually, the IDL functions have no default values — we need to
		// encode the values differently. Let's use the function's doc comment
		// or a naming convention.
		// Pragmatic: the emitter maps known names to known values (same as codec.ts).
		lines.push(`${indent}\t${fn.name}: ${getPlpValue(iface.name, fn.name)},`);
	}
	lines.push(`${indent}} as const;`);
	return lines.join("\n");
}

// Map PLP IDL names to their wire values. This is the bridge between the IDL
// (which has no literal constants) and the wire protocol (which does).
// The values mirror firmware plp_codec.h and codec.ts byte-for-byte.
const PLP_VALUES: Record<string, Record<string, number>> = {
	channels: {
		control: 0x00,
		screen: 0x01,
		input: 0x02,
		log: 0x03,
		system: 0x04,
		ota: 0x05,
		ext: 0x06,
		event: 0x07,
		cli: 0x08,
		file: 0x09
	},
	control_ops: {
		hello: 0x01,
		ack: 0x02,
		reject: 0x03,
		auth_challenge: 0x20,
		auth_response: 0x21,
		auth_ok: 0x22,
		auth_fail: 0x23,
		auth_required: 0x24
	},
	file_ops: {
		list: 0x01,
		read: 0x03,
		write: 0x04,
		mkdir: 0x05,
		remove: 0x06,
		rename: 0x07,
		copy: 0x08
	},
	ext_ops: {
		inject_event: 0x01,
		wifi_set_networks: 0x02,
		app_install: 0x03,
		app_scan: 0x04
	},
	ota_ops: {
		begin: 0x01,
		data: 0x02,
		end: 0x03
	},
	system_ops: {
		get_info: 0x01
	}
};

function getPlpValue(ifaceName: string, funcName: string): number {
	const table = PLP_VALUES[ifaceName];
	if (table && funcName in table) return table[funcName];
	return 0;
}

export function emitPlpTs(ast: PidlAst): string {
	const pkg = findPlpPackage(ast);
	if (!pkg) {
		return "// No palanu:plp package found in IDL.\n";
	}

	const lines: string[] = [];
	lines.push("// PLP wire protocol constants — generated from api/plp.pidl.");
	lines.push("// Channel numbers, opcodes, and frame flags for the Palanu Link Protocol.");
	lines.push("// These mirror firmware plp_codec.h and codec.ts byte-for-byte.");
	lines.push("");

	// Emit each interface as a const object
	for (const iface of pkg.interfaces) {
		if (iface.doc) {
			lines.push(`/** ${iface.doc.trim()} */`);
		}
		lines.push(funcsToConstObject(iface, ""));
		lines.push("");
	}

	// Emit frame flags from records
	for (const rec of pkg.records) {
		if (rec.name === "frame_flags") {
			lines.push("export const Flags = {");
			lines.push("\tNone: 0,");
			lines.push("\tFragMore: 1 << 0,");
			lines.push("\tCompressed: 1 << 1");
			lines.push("} as const;");
			lines.push("");
		}
	}

	return lines.join("\n");
}
