import type { Handle } from '@sveltejs/kit';

// Cross-origin isolation for SharedArrayBuffer (WASM firmware pthreads).
// Mirrors the dev/preview headers in vite.config.ts for the production server.
export const handle: Handle = async ({ event, resolve }) => {
	const res = await resolve(event);
	res.headers.set('Cross-Origin-Opener-Policy', 'same-origin');
	res.headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
	return res;
};
