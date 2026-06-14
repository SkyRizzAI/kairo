import type { RequestHandler } from './$types';

const ALLOWED_ORIGINS = [
	'https://github.com/',
	'https://objects.githubusercontent.com/',
	'https://github-releases.githubusercontent.com/',
];

export const GET: RequestHandler = async ({ url }) => {
	const target = url.searchParams.get('url');
	if (!target) return new Response('missing url parameter', { status: 400 });

	if (!ALLOWED_ORIGINS.some((o) => target.startsWith(o))) {
		return new Response('only GitHub release URLs are allowed', { status: 403 });
	}

	let upstream: Response;
	try {
		upstream = await fetch(target, {
			headers: { 'User-Agent': 'Forge-firmware-proxy/1.0' },
			redirect: 'follow'
		});
	} catch (e) {
		const detail = e instanceof Error
			? `${e.name}: ${e.message}${e.cause ? ` (cause: ${e.cause})` : ''}`
			: String(e);
		return new Response(`upstream fetch failed: ${detail}`, { status: 502 });
	}

	if (!upstream.ok) {
		return new Response(`upstream error: HTTP ${upstream.status}`, { status: upstream.status });
	}

	const headers = new Headers({
		'Content-Type': 'application/octet-stream',
		'Cache-Control': 'no-store'
	});
	const length = upstream.headers.get('Content-Length');
	if (length) headers.set('Content-Length', length);

	return new Response(upstream.body, { headers });
};
