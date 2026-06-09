import type { RequestHandler } from './$types';
import { fetchRequestHandler } from '@trpc/server/adapters/fetch';
import { appRouter } from '$lib/trpc/router';
import { createContext } from '$lib/trpc/context';

// Single fetch-adapter handler for all tRPC procedures (queries, mutations,
// and SSE subscriptions).
const handler: RequestHandler = (event) =>
	fetchRequestHandler({
		endpoint: '/api/trpc',
		req: event.request,
		router: appRouter,
		createContext: () => createContext(event)
	});

export const GET = handler;
export const POST = handler;
