import tailwindcss from '@tailwindcss/vite';
import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

// Cross-origin isolation headers — required for SharedArrayBuffer (WASM firmware
// pthreads). The wasm assets themselves are served by /routes/fw/[file] which
// also sets COEP + Cross-Origin-Resource-Policy (needed for pthread workers).
const coiHeaders = {
	'Cross-Origin-Opener-Policy': 'same-origin',
	'Cross-Origin-Embedder-Policy': 'require-corp'
};

export default defineConfig({
	plugins: [tailwindcss(), sveltekit()],
	server: { headers: coiHeaders },
	preview: { headers: coiHeaders }
});
