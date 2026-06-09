import { readFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { router, publicProcedure } from './init';

// tRPC API. The simulator is now WASM (client-side) — no server-side sim bridge.
// The only server-truth surface is the firmware registry: list the firmware
// bundles available for the web flasher (/flash, Web Serial) and OTA pull. The
// bundles + manifest are produced by firmware/tools/publish-firmware.sh and live
// under static/firmware/ (served as plain static assets).

export interface FirmwarePart {
	offset: string;
	path: string;
}
export interface FirmwareBuild {
	id: string;
	board: string;
	chip: string;
	version: string;
	appSize: number;
	flash: { mode: string; freq: string; size: string };
	parts: FirmwarePart[];
}

function readManifest(): { version: string; builds: FirmwareBuild[] } {
	// cwd is packages/forge in dev, build/ in adapter-node prod.
	const candidates = [
		'static/firmware/manifest.json',
		'client/firmware/manifest.json',
		'build/client/firmware/manifest.json'
	];
	const fp = candidates.map((c) => resolve(c)).find(existsSync);
	if (!fp) return { version: 'none', builds: [] };
	try {
		return JSON.parse(readFileSync(fp, 'utf8'));
	} catch {
		return { version: 'none', builds: [] };
	}
}

export const appRouter = router({
	firmware: router({
		// All available firmware builds (board, version, flash parts + offsets).
		list: publicProcedure.query((): FirmwareBuild[] => readManifest().builds),
		// Manifest version (git describe) — for a freshness/OTA check.
		version: publicProcedure.query((): string => readManifest().version)
	})
});

export type AppRouter = typeof appRouter;
