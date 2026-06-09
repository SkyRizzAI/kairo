import {
	createTRPCClient,
	httpBatchLink,
	httpSubscriptionLink,
	splitLink
} from '@trpc/client';
import type { AppRouter } from './router';

// Browser tRPC client. Subscriptions go over SSE (httpSubscriptionLink),
// everything else is batched HTTP. `AppRouter` is a type-only import, so no
// server code leaks into the browser bundle.
export const trpc = createTRPCClient<AppRouter>({
	links: [
		splitLink({
			condition: (op) => op.type === 'subscription',
			true: httpSubscriptionLink({ url: '/api/trpc' }),
			false: httpBatchLink({ url: '/api/trpc' })
		})
	]
});
