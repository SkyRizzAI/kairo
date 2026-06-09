import { readFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { error } from '@sveltejs/kit';
import type { RequestHandler } from './$types';

// Serve the WASM firmware (kairo.js / kairo.wasm) with the headers emscripten
// pthread workers REQUIRE under cross-origin isolation. Creating a Worker from a
// script that lacks Cross-Origin-Resource-Policy fails opaquely on a COI page
// (worker never replies → onRuntimeInitialized never fires → "loading-workers"
// hang). Static handlers (Vite publicDir / adapter-node sirv) don't add CORP, so
// we serve through this endpoint — guaranteed headers in BOTH dev and prod.
export const GET: RequestHandler = ({ params }) => {
	const file = params.file ?? '';
	if (!/^kairo\.(js|wasm)$/.test(file)) throw error(404, 'not found');

	const candidates = [
		`static/wasm/${file}`, // dev (cwd = packages/forge)
		`client/wasm/${file}`, // adapter-node prod (cwd = build/)
		`build/client/wasm/${file}` // prod from package root
	];
	const fp = candidates.map((c) => resolve(c)).find(existsSync);
	if (!fp) throw error(404, `wasm asset not built: ${file} (run bun run build:wasm)`);

	const body = readFileSync(fp);
	return new Response(body, {
		headers: {
			'Content-Type': file.endsWith('.wasm') ? 'application/wasm' : 'text/javascript',
			'Cross-Origin-Embedder-Policy': 'require-corp',
			'Cross-Origin-Resource-Policy': 'same-origin',
			'Cross-Origin-Opener-Policy': 'same-origin',
			'Cache-Control': 'no-cache'
		}
	});
};
